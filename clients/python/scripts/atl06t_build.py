import os
import json
import argparse
import duckdb
import boto3
import functools
import operator
import numpy as np
import pyarrow as pa
import pyarrow.dataset as ds
import pyarrow.parquet as pq
import geopandas as gpd
from geopandas.io.arrow import _geopandas_to_arrow
from sliderule import sliderule

# #####################################
# Globals
# #####################################

# constants
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
ROW_GROUP_RANGE_S2_CELL_LEVEL = 13 # 1.27 km^2
ROW_GROUP_TARGET_SIZE_MB = 5 # MB

# create S3 client
s3 = boto3.client("s3")

# #####################################
# Functions
# #####################################

# Split out bucket and key from url
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    key = '/'.join(path.split("/")[1:])
    return bucket, key

# list files in an s3 bucket
def list_bucket(url):
    filenames = []
    bucket, prefix = parse_url(url)
    is_truncated = True
    continuation_token = None
    while is_truncated:
        # make request
        if continuation_token:
            response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix, ContinuationToken=continuation_token)
        else:
            response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix)
        # parse contents
        if 'Contents' in response:
            for obj in response['Contents']:
                filenames.append(f"{bucket}/{obj['Key']}")
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    return filenames

# list subfolders (one level deep) in an s3 bucket
def list_subfolders(url):
    subfolders = []
    bucket, prefix = parse_url(url)
    if prefix and not prefix.endswith("/"):
        prefix += "/"
    paginator = s3.get_paginator("list_objects_v2")
    # the "/" delimiter makes S3 group keys by their next path segment into CommonPrefixes
    for page in paginator.paginate(Bucket=bucket, Prefix=prefix, Delimiter="/"):
        for common_prefix in page.get("CommonPrefixes", []):
            subfolders.append(f"{bucket}/{common_prefix['Prefix']}")
    return subfolders

# assign cell IDs
def assign_cell_ids(gdf, level=18):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    coords = gpd.pd.DataFrame({"lon": geometry.x.to_numpy(), "lat": geometry.y.to_numpy()})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# write optimized parquet
def write_optimized_parquet(gdf, output_file, target_bytes=ROW_GROUP_TARGET_SIZE_MB*1048576):
    """
    rows per row group adapt to the bytes actually written, so each group lands near target_bytes
    row groups are extended to the end of its last cell_id so no cell spans two groups
    """
    table = _geopandas_to_arrow(gdf, index=True)
    cell_idx = table.schema.get_field_index("cell_id")
    cell_ids = gdf.index
    with pa.OSFile(output_file, "wb") as sink:
        with pq.ParquetWriter(sink, table.schema, sorting_columns=[pq.SortingColumn(cell_idx)]) as writer:
            start, rows = 0, 50_000
            while start < table.num_rows:
                end = min(start + rows, table.num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(table.slice(start, end - start), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# generate optimal range filter
def gen_optimized_range_filter(cover_ids, tile_url, range_level=ROW_GROUP_RANGE_S2_CELL_LEVEL):
    # every child level descendant of a parent level cell c lies in [c - half, c + half]
    half = (1 << (2 * (30 - range_level))) - 1
    exact_ranges = []
    for c in map(int, cover_ids):
        if exact_ranges and c - half <= exact_ranges[-1][1] + 2:  # Hilbert-adjacent cells form one range
            exact_ranges[-1][1] = c + half
        else:
            exact_ranges.append([c - half, c + half])
    # query metadata for row group boundaries
    file_meta = pq.ParquetFile(tile_url).metadata
    cid = file_meta.schema.to_arrow_schema().get_field_index("cell_id")
    rg_bounds = [(file_meta.row_group(i).column(cid).statistics.min, file_meta.row_group(i).column(cid).statistics.max)
                for i in range(file_meta.num_row_groups)]
    # merge across a gap unless a whole row group sits inside it, so merging never adds a row group to the read
    read_ranges = []
    def row_groups_hit(ranges):
        return [i for i, (mn, mx) in enumerate(rg_bounds) if any(lo <= mx and mn <= hi for lo, hi in ranges)]
    for lo, hi in (r for r in exact_ranges if row_groups_hit([r])):
        if read_ranges and not any(read_ranges[-1][1] < mn and mx < lo for mn, mx in rg_bounds):
            read_ranges[-1][1] = hi
        else:
            read_ranges.append([lo, hi])
    # build range filter
    f = ds.field("cell_id")
    range_flt = functools.reduce(operator.or_, [
        (f >= pa.scalar(lo, pa.uint64())) & (f <= pa.scalar(hi, pa.uint64())) for lo, hi in read_ranges
    ])

# trim geodataframe to area of interest
def trim_to_aoi(gdf, connection, wkt, range_cover_ids, exact=True, segment_level=SEGMENT_S2_CELL_LEVEL, range_level=ROW_GROUP_RANGE_S2_CELL_LEVEL):
    connection.execute("CREATE OR REPLACE TEMP TABLE poly_geog AS SELECT s2_prepare(s2_geogfromtext(?)) AS g", [wkt])
    # cells fully inside the polygon are kept at their level; cells crossing its edge are split down to the segment level
    interior, cells = [], range_cover_ids
    for lvl in range(range_level, segment_level+1):
        connection.register("cells", gpd.pd.DataFrame({"c": cells}))
        res = connection.sql("""
            SELECT c, s2_contains(p.g, c::S2_CELL::GEOGRAPHY) AS inside, s2_intersects(p.g, c::S2_CELL::GEOGRAPHY) AS touches
            FROM cells, poly_geog p
        """).fetchnumpy()
        interior.append(res["c"][res["inside"]])
        cells = res["c"][res["touches"] & ~res["inside"]]
        if lvl < 18:
            lsb = cells & (~cells + np.uint64(1))
            cells = np.sort(np.concatenate([cells - lsb + (lsb >> np.uint64(2)) * np.uint64(2 * k + 1) for k in range(4)]))
    interior = np.sort(np.concatenate(interior))
    boundary = cells
    # build rows to keep
    ids = gdf.index.to_numpy()
    keep = np.zeros(len(ids), bool)
    if len(interior):
        ext = (interior & (~interior + np.uint64(1))) - np.uint64(1)  # each cell spans [c - ext, c + ext]
        k = np.searchsorted(interior - ext, ids, side="right") - 1
        keep = (k >= 0) & (ids <= (interior + ext)[np.maximum(k, 0)])
    # only points in segment level cells crossing the polygon edge need an exact point-in-polygon test
    if exact:
        on_boundary = np.isin(ids, boundary)
        ll = gdf.geometry[on_boundary].to_crs("EPSG:4326")
        connection.register("pts", gpd.pd.DataFrame({"lon": ll.x.to_numpy(), "lat": ll.y.to_numpy()}))
        keep[on_boundary] = connection.sql(
            "SELECT s2_contains(p.g, s2_cellfromlonlat(lon, lat)::GEOGRAPHY) AS inside FROM pts, poly_geog p"
        ).fetchnumpy()["inside"]
    # return mask to trim gdf
    return keep

# query cmr for granules
def query_cmr(aoi, output, domain, cluster, verbose):
    session = sliderule.create_session(domain=domain, cluster=cluster, verbose=verbose, rqst_timeout=(10,300))
    region = sliderule.toregion(aoi)
    granules = session.source("earthdata", {
        "asset": "icesat2-atl06",
        "poly": region["poly"],
        "max_resources": 500000,
    }, rethrow=True)
    if isinstance(granules, list) and len(granules) > 0:
        print(f"Writing {len(granules)} granules to {output}")
        with open(output, "w") as file:
            json.dump(granules, file)
    else:
        print(f"Failed query for granules: {granules}")

# write S2 partitioned parquets
def write_s2_partitions(atl06_granules_file, bucket, prefix, domain, cluster, verbose, level=TILE_S2_CELL_LEVEL):
    session = sliderule.create_session(domain=domain, cluster=cluster, verbose=verbose)
    with open(atl06_granules_file, "r") as file:
        granules = json.load(file)
    for i in range(len(granules[:20])):
        granule = granules[i]
        print(f"Partitioning [{i}/{len(granules)}] {granule}")
        gdf = sliderule.run("atl06x", {}, resources=[granule], session=session)
        gdf["cell_id"] = assign_cell_ids(gdf, level)
        gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")
        for cell_id in gdf.index.unique():
            l7_gdf = gdf[gdf.index == cell_id]
            local_parquet_file = f"/tmp/{granule.replace('.h5', '')}.S{cell_id:016X}.parquet"
            remote_parquet_file = f"{prefix}/partitions/S{cell_id:016X}/{granule.replace('.h5', '.parquet')}"
            l7_gdf.to_parquet(local_parquet_file, index=False)
            s3.upload_file(local_parquet_file, bucket, remote_parquet_file)
            os.remove(local_parquet_file)

# write ATL06 tiles
def write_atl06_tiles(bucket, prefix, level=SEGMENT_S2_CELL_LEVEL):
    subfolders = list_subfolders(f"s3://{bucket}/{prefix}/partitions/")
    for i in range(len(subfolders)):
        subfolder = subfolders[i]
        cell_id_str = subfolder.split("/")[-2]
        filenames = list_bucket(f"s3://{subfolder}")
        print(f"Tiling [{i}/{len(subfolders)}] {cell_id_str} with {len(filenames)} files")
        gdfs = [gpd.read_parquet(f"s3://{filename}") for filename in filenames]
        if len(gdfs) > 0:
            gdf = gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
            gdf["cell_id"] = assign_cell_ids(gdf, level)
            gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")
            local_parquet_file = f"/tmp/ATL06T_{cell_id_str}.parquet"
            remote_parquet_file = f"{prefix}/tiles/ATL06T_{cell_id_str}.parquet"
            write_optimized_parquet(gdf, local_parquet_file)
            s3.upload_file(local_parquet_file, bucket, remote_parquet_file)
            os.remove(local_parquet_file)

# read ATL06 tiles
def read_atl06_tiles(aoi, bucket, prefix, exact, tile_level=TILE_S2_CELL_LEVEL, range_level=ROW_GROUP_RANGE_S2_CELL_LEVEL):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    # get set of S2 cells that cover area of interest
    region = sliderule.toregion(aoi)
    wkt = "POLYGON((" + ", ".join(f"{p['lon']} {p['lat']}" for p in region["poly"]) + "))"
    tile_cover_ids = [row[0] for row in connection.execute(
        f"SELECT UNNEST(s2_covering_fixed_level(s2_geogfromtext(?), {tile_level}))::UBIGINT", [wkt]).fetchall()
    ]
    # read all tiles corresponding to an S2 cell in the area of interest
    gdfs = []
    for i in range(len(tile_cover_ids)):
        tile_id = tile_cover_ids[i]
        print(f"Reading [{i}/{len(tile_cover_ids)}] tile {tile_id:016X}")
        range_cover_ids = connection.execute(f"""
            WITH tile AS (SELECT {tile_id}::UBIGINT::S2_CELL AS cell)
            SELECT c::UBIGINT AS cell_id
            FROM tile, UNNEST(s2_covering_fixed_level(s2_intersection(s2_geogfromtext(?), tile.cell::GEOGRAPHY), {range_level})) AS t(c)
            WHERE c::UBIGINT BETWEEN s2_cell_range_min(tile.cell)::UBIGINT AND s2_cell_range_max(tile.cell)::UBIGINT
            ORDER BY cell_id
        """, [wkt]).fetchnumpy()["cell_id"]
        tile_url = f"s3://{bucket}/{prefix}/tiles/ATL06T_S{tile_id:016X}.parquet"
        range_flt = gen_optimized_range_filter(range_cover_ids, tile_url)
        gdf = gpd.read_parquet(tile_url, filters=range_flt)
        gdfs.append(gdf)
    # build final gdf and trim to area of interest
    gdf = gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
    keep = trim_to_aoi(gdf, connection, wkt, range_cover_ids, exact)
    return gdf[keep]

# #####################################
# Main
# #####################################

if __name__ == "__main__":

    # command line arguments
    parser = argparse.ArgumentParser(description="""ATL06 Tile Builder""")
    parser.add_argument('--domain',         type=str,               default="localhost")
    parser.add_argument('--cluster',        type=str,               default=None)
    parser.add_argument('--atl06_granules', type=str,               default="/data/ATL06/atl06_granules_greenland.json")
    parser.add_argument('--output_bucket',  type=str,               default="sliderule-public")
    parser.add_argument('--output_prefix',  type=str,               default="atl06t")
    parser.add_argument('--aoi',            type=str,               default="/data/ATL06/greenland.geojson")
    parser.add_argument('--aoi_output',     type=str,               default="/data/ATL06/atl06_granules.json")
    parser.add_argument('--start',          type=str,               default=None) # YYYY-MM-DDTHH:MM:SS
    parser.add_argument('--end',            type=str,               default=None) # YYYY-MM-DDTHH:MM:SS
    parser.add_argument('--exact',          action='store_true',    default=False)
    parser.add_argument('--verbose',        action='store_true',    default=False)
    parser.add_argument('--query',          action='store_true',    default=False) # create list of granules to process (uses aoi and writes atl06_granules)
    parser.add_argument('--partition',      action='store_true',    default=False) # partition granules into cells (reads atl06_granules)
    parser.add_argument('--tile',           action='store_true',    default=False) # tile partitions back into new tiled granules
    parser.add_argument('--read',           action='store_true',    default=False) # read tiled granules
    args = parser.parse_args()

    # route command
    if args.query:          query_cmr(args.aoi, args.output, args.domain, args.cluster, args.verbose)
    elif args.partition:    write_s2_partitions(args.atl06_granules, args.output_bucket, args.output_prefix, args.domain, args.cluster, args.verbose)
    elif args.tile:         write_atl06_tiles(args.output_bucket, args.output_prefix)
    elif args.read:         print(read_atl06_tiles(args.aoi, args.output_bucket, args.output_prefix, args.exact))