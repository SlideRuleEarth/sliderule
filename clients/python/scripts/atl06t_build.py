import os
import json
import argparse
import duckdb
import boto3
import numpy as np
import pyarrow as pa
import pyarrow.fs as pafs
import pyarrow.parquet as pq
import geopandas as gpd
from concurrent.futures import ThreadPoolExecutor
from geopandas.io.arrow import _geopandas_to_arrow, _arrow_to_geopandas
from sliderule import sliderule

# #####################################
# Globals
# #####################################

# constants
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
ROW_GROUP_TARGET_SIZE_MB = 2 # MB
S3_REGION = "us-west-2"

# create S3 client
s3 = boto3.client("s3")

# #####################################
# Helper Functions
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
    floats = [f.name for f in table.schema if pa.types.is_floating(f.type)]
    with pa.OSFile(output_file, "wb") as sink:
        with pq.ParquetWriter(sink, table.schema,
                              sorting_columns=[pq.SortingColumn(cell_idx)],
                              compression="zstd",
                              # byte_stream_split only takes effect on columns without dictionary encoding
                              use_dictionary=[name for name in table.column_names if name not in floats],
                              use_byte_stream_split=floats,
                              write_statistics=["cell_id", "time_ns"]) as writer:
            start, rows = 0, 50_000
            while start < table.num_rows:
                end = min(start + rows, table.num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(table.slice(start, end - start), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# [lo, hi] range of cell ids covered by each cell (any level)
def cell_ranges(cells):
    ext = (cells & (~cells + np.uint64(1))) - np.uint64(1)
    return cells - ext, cells + ext

# mask of ids inside any of the sorted, disjoint [lo, hi] ranges
def in_ranges(ids, lo, hi):
    if len(lo) == 0:
        return np.zeros(len(ids), bool)
    k = np.searchsorted(lo, ids, side="right") - 1
    return (k >= 0) & (ids <= hi[np.maximum(k, 0)])

# split a tile into cells fully inside the aoi (mixed levels) and segment level cells crossing its edge
def classify_cells(cursor, tile_id, tile_level=TILE_S2_CELL_LEVEL, segment_level=SEGMENT_S2_CELL_LEVEL):
    interior, cells = [], np.array([tile_id], dtype=np.uint64)
    for lvl in range(tile_level, segment_level + 1):
        if len(cells) == 0:
            break
        cursor.register("cells", gpd.pd.DataFrame({"c": cells}))
        res = cursor.sql("""
            SELECT c, s2_contains(p.g, c::S2_CELL::GEOGRAPHY) AS inside, s2_intersects(p.g, c::S2_CELL::GEOGRAPHY) AS touches
            FROM cells, poly_geog p
        """).fetchnumpy()
        interior.append(res["c"][res["inside"]])
        cells = res["c"][res["touches"] & ~res["inside"]]
        if lvl < segment_level:
            lsb = cells & (~cells + np.uint64(1))
            cells = np.sort(np.concatenate([cells - lsb + (lsb >> np.uint64(2)) * np.uint64(2 * k + 1) for k in range(4)]))
    return np.sort(np.concatenate(interior)), cells

# select row groups whose cell_id min/max overlaps any of the sorted, disjoint [lo, hi] ranges
def select_row_groups(lo, hi, file_meta):
    cid = file_meta.schema.to_arrow_schema().get_field_index("cell_id")
    stats = [file_meta.row_group(i).column(cid).statistics for i in range(file_meta.num_row_groups)]
    rg_min = np.array([s.min for s in stats], dtype=np.uint64)
    rg_max = np.array([s.max for s in stats], dtype=np.uint64)
    # first range ending at or after the row group's min; they overlap if that range starts before the row group's max
    k = np.searchsorted(hi, rg_min, side="left")
    hit = k < len(hi)
    hit[hit] = lo[k[hit]] <= rg_max[hit]
    return np.flatnonzero(hit).tolist()

# trim geodataframe to area of interest
def trim_to_aoi(gdf, cursor, interior, boundary, exact=True):
    ids = gdf.index.to_numpy()
    keep = in_ranges(ids, *cell_ranges(interior))
    on_boundary = np.isin(ids, boundary)
    if not exact:
        keep |= on_boundary
    elif on_boundary.any():
        # only points in segment level cells crossing the polygon edge need an exact point-in-polygon test
        ll = gdf.geometry[on_boundary].to_crs("EPSG:4326")
        cursor.register("pts", gpd.pd.DataFrame({"lon": ll.x.to_numpy(), "lat": ll.y.to_numpy()}))
        keep[on_boundary] = cursor.sql(
            "SELECT s2_contains(p.g, s2_cellfromlonlat(lon, lat)::GEOGRAPHY) AS inside FROM pts, poly_geog p"
        ).fetchnumpy()["inside"]
    return gdf[keep]

# filter geodataframe with time range
def filter_time(gdf, start, end, time_column="time_ns"):
    if start is None and end is None:
        return gdf
    times = gdf[time_column]
    def to_timestamp(t):
        # naive inputs are taken as UTC; result matches the column's tz-awareness so comparisons work
        ts = gpd.pd.Timestamp(t)
        ts = ts.tz_localize("UTC") if ts.tzinfo is None else ts.tz_convert("UTC")
        return ts if times.dt.tz is not None else ts.tz_localize(None)
    t0 = to_timestamp(start if start is not None else "2018-10-01")
    t1 = to_timestamp(end if end is not None else gpd.pd.Timestamp.now(tz="UTC"))
    return gdf[times.between(t0, t1, inclusive="both")]

# #####################################
# Command Functions
# #####################################

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
            gdf = gdf.set_index("cell_id").sort_index(kind="stable")
            local_parquet_file = f"/tmp/ATL06T_{cell_id_str}.parquet"
            remote_parquet_file = f"{prefix}/tiles/ATL06T_{cell_id_str}.parquet"
            write_optimized_parquet(gdf, local_parquet_file)
            s3.upload_file(local_parquet_file, bucket, remote_parquet_file)
            os.remove(local_parquet_file)

# read the rows of one tile inside the area of interest; returns (gdf or None, status)
def read_atl06_tile(connection, s3_fs, tile_id, bucket, prefix, start, end, exact, tile_level=TILE_S2_CELL_LEVEL, segment_level=SEGMENT_S2_CELL_LEVEL):
    try:
        # pre_buffer coalesces the selected column chunks into a few concurrent range requests
        pf = pq.ParquetFile(f"{bucket}/{prefix}/tiles/ATL06T_S{tile_id:016X}.parquet", filesystem=s3_fs, pre_buffer=True)
    except FileNotFoundError:
        return None, "not found"
    # duckdb connections are not thread safe; a cursor is a separate connection to the same database
    cursor = connection.cursor()
    try:
        interior, boundary = classify_cells(cursor, tile_id, tile_level, segment_level)
        lo, hi = cell_ranges(np.sort(np.concatenate([interior, boundary])))
        row_groups = select_row_groups(lo, hi, pf.metadata)
        if not row_groups:
            return None, "empty"
        gdf = _arrow_to_geopandas(pf.read_row_groups(row_groups, use_pandas_metadata=True))
        gdf = filter_time(gdf, start, end)
        gdf = trim_to_aoi(gdf, cursor, interior, boundary, exact)
        return gdf, f"{len(gdf)} rows from {len(row_groups)}/{pf.metadata.num_row_groups} row groups"
    finally:
        cursor.close()
        pf.close()

# read ATL06 tiles
def read_atl06_tiles(aoi, start, end, bucket, prefix, exact, anonymous=True, max_workers=8, tile_level=TILE_S2_CELL_LEVEL, segment_level=SEGMENT_S2_CELL_LEVEL):
    # anonymous reads skip credential lookup, and a fixed region skips per-file region resolution
    s3_fs = pafs.S3FileSystem(region=S3_REGION, anonymous=anonymous)
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    # get set of S2 cells that cover area of interest
    aoi_gdf = sliderule.toregion(aoi)["gdf"]
    if aoi_gdf.crs is not None:
        aoi_gdf = aoi_gdf.to_crs("EPSG:4326")
    wkt = aoi_gdf.union_all().wkt
    # a regular (not TEMP) table so the per-thread cursors can see it
    connection.execute("CREATE OR REPLACE TABLE poly_geog AS SELECT s2_prepare(s2_geogfromtext(?)) AS g", [wkt])
    tile_cover_ids = [row[0] for row in connection.execute(
        f"SELECT UNNEST(s2_covering_fixed_level(s2_geogfromtext(?), {tile_level}))::UBIGINT", [wkt]).fetchall()
    ]
    # read all tiles corresponding to an S2 cell in the area of interest
    gdfs = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        results = executor.map(
            lambda tile_id: read_atl06_tile(connection, s3_fs, tile_id, bucket, prefix, start, end, exact, tile_level, segment_level),
            tile_cover_ids)
        for i, (tile_id, (gdf, status)) in enumerate(zip(tile_cover_ids, results)):
            print(f"Read [{i+1}/{len(tile_cover_ids)}] tile {tile_id:016X} - {status}")
            if gdf is not None:
                gdfs.append(gdf)
    # combine tiles
    if len(gdfs) > 0:
        return gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
    else:
        return None

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
    parser.add_argument('--signed',         action='store_true',    default=False) # read tiles with AWS credentials instead of anonymously
    parser.add_argument('--verbose',        action='store_true',    default=False)
    parser.add_argument('--query',          action='store_true',    default=False) # create list of granules to process (uses aoi and writes atl06_granules)
    parser.add_argument('--partition',      action='store_true',    default=False) # partition granules into cells (reads atl06_granules)
    parser.add_argument('--tile',           action='store_true',    default=False) # tile partitions back into new tiled granules
    parser.add_argument('--read',           action='store_true',    default=False) # read tiled granules
    args = parser.parse_args()

    # route command
    if args.query:          query_cmr(args.aoi, args.aoi_output, args.domain, args.cluster, args.verbose)
    elif args.partition:    write_s2_partitions(args.atl06_granules, args.output_bucket, args.output_prefix, args.domain, args.cluster, args.verbose)
    elif args.tile:         write_atl06_tiles(args.output_bucket, args.output_prefix)
    elif args.read:         print(read_atl06_tiles(args.aoi, args.start, args.end, args.output_bucket, args.output_prefix, args.exact, anonymous=not args.signed))