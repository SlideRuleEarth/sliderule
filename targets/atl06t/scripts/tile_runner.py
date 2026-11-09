import os
import sys
import json
import time
import duckdb
import traceback
from concurrent.futures import ThreadPoolExecutor
import boto3
import s3fs
import numpy as np
import shapely
import pyarrow as pa
import pyarrow.parquet as pq
import geopandas as gpd
from geopandas.io.arrow import _geopandas_to_arrow, _arrow_to_geopandas, _validate_and_decode_metadata
from sliderule import pysr

# runner
runner = pysr.Runner()
partition, result_file = runner.arguments

# constants
OUTPUT_BUCKET = "sliderule-public"
OUTPUT_PREFIX = "atl06tv4"
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
ROW_GROUP_TARGET_SIZE_MB = 8 # MB
ROW_BYTES_ESTIMATE = 150 # bytes; estimated compressed size of a row, only sizes the first row group
WKB_CHUNK_LIMIT = 2**31 - 2**24 # bytes; safely below the ~2 GiB at which pyarrow splits a binary column into multiple chunks
FIRST_COLUMNS = [ # written first in each row group so reads of them are contiguous
    "cell_id",
    "geometry",
    "time_ns",
    "atl06_quality_summary",
    "delta_time",
    "h_li",
    "h_li_sigma",
    "segment_id",
    "h_rms_misfit",
    "h_robust_sprd",
    "n_fit_photons",
    "msw_flag",
    "x_atc",
    "y_atc",
    "dh_fit_dx",
    "dh_fit_dy"
]

# create S3 client
s3 = boto3.client("s3")

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

# assign cell IDs
def assign_cell_ids(lon, lat, level=18):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    coords = gpd.pd.DataFrame({"lon": lon, "lat": lat})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# original approach: holds the whole tile as a GeoDataFrame (several copies at peak)
def build_in_memory(filenames):
    gdfs = [gpd.read_parquet(f"s3://{filename}") for filename in filenames]
    gdf = gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
    del gdfs
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    gdf["cell_id"] = assign_cell_ids(geometry.x.to_numpy(), geometry.y.to_numpy(), SEGMENT_S2_CELL_LEVEL)
    del geometry
    gdf = gdf.set_index("cell_id").sort_index(kind="stable")
    table = _geopandas_to_arrow(gdf, index=True)
    cell_ids = gdf.index.to_numpy()
    del gdf
    return table.schema, cell_ids, lambda start, end: table.slice(start, end - start)

# low memory approach: copies each file into preallocated numpy columns as it is read (so the tile is never held
# twice), and only builds a GeoDataFrame one row group at a time; every conversion is the same one
# build_in_memory performs, so the parquet file is byte-for-byte identical; returns None when that can't be
# guaranteed so the caller can fall back to build_in_memory
def build_streaming(filenames):
    fs = s3fs.S3FileSystem()
    with ThreadPoolExecutor(max_workers=16) as executor:
        file_rows = list(executor.map(lambda filename: pq.read_metadata(filename, filesystem=fs).num_rows, filenames))
    num_rows = sum(file_rows)
    columns = {} # non-geometry columns, preallocated
    wkb_chunks = []
    lon, lat = np.empty(num_rows), np.empty(num_rows)
    schema, geo_metadata, geometry_column = None, None, None
    extremes = [None] * 4 # (value, row) of each component of the bounding box: minx, miny, maxx, maxy
    type_rows = {} # first row of each geometry type
    offset = 0
    for filename, rows in zip(filenames, file_rows):
        table = pq.read_table(filename, filesystem=fs)
        file_geo_metadata = _validate_and_decode_metadata(table.schema.metadata)
        if schema is None:
            schema = table.schema
            geo_metadata = file_geo_metadata
            geometry_column = geo_metadata["primary_column"]
            if len(geo_metadata["columns"]) != 1 or geo_metadata["columns"][geometry_column]["encoding"] != "WKB":
                runner.message(f"unsupported geo metadata: {len(geo_metadata["columns"])} {geo_metadata["columns"][geometry_column]["encoding"]}")
                return None
            reference_crs = json.dumps(geo_metadata["columns"][geometry_column].get("crs"), sort_keys=True)
            for field in schema:
                if field.name == geometry_column:
                    continue
                if not (pa.types.is_integer(field.type) or pa.types.is_floating(field.type) or pa.types.is_boolean(field.type) or \
                        (pa.types.is_timestamp(field.type) and field.type.tz is None)):
                    runner.message(f"unsupported field type: {field.type}")
                    return None
                columns[field.name] = np.empty(num_rows, dtype=field.type.to_pandas_dtype())
        elif not table.schema.equals(schema) or \
             file_geo_metadata["primary_column"] != geometry_column or \
             len(file_geo_metadata["columns"]) != 1 or \
             file_geo_metadata["columns"][geometry_column]["encoding"] != "WKB" or \
             json.dumps(file_geo_metadata["columns"][geometry_column].get("crs"), sort_keys=True) != reference_crs:
            # pandas would upcast mismatched columns on concat
            runner.message(f"mismatched schema")
            return None
        if table.num_rows != rows:
            runner.message(f"mismatched rows: {table.num_rows} != {rows}")
            return None
        for name, column in columns.items():
            values = table.column(name)
            # nulls in integer and boolean columns change the pandas dtype per file (e.g. int -> float64)
            if values.null_count > 0 and not (pa.types.is_floating(values.type) or pa.types.is_timestamp(values.type)):
                runner.message(f"unsupported null elements: {values.null_count}")
                return None
            # nulls become NaN / NaT, as they do when converting to pandas
            column[offset:offset + rows] = values.to_numpy()
        wkb_chunks.extend(table.column(geometry_column).chunks)
        # same conversion gpd.read_parquet performs, for the geometry column only
        geometry = _arrow_to_geopandas(table.select([geometry_column]), geo_metadata).geometry
        del table
        lonlat = geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
        lon[offset:offset + rows] = lonlat.x.to_numpy()
        lat[offset:offset + rows] = lonlat.y.to_numpy()
        del lonlat
        # rows that determine the bbox and geometry types written to the geo metadata
        geoms = geometry.array._data
        bounds = shapely.bounds(geoms)
        for k in range(4):
            values = bounds[:, k]
            if np.isnan(values).all():
                continue
            i = int(np.nanargmin(values) if k < 2 else np.nanargmax(values))
            if extremes[k] is None or (values[i] < extremes[k][0] if k < 2 else values[i] > extremes[k][0]):
                extremes[k] = (values[i], offset + i)
        type_ids = shapely.get_type_id(geoms)
        type_ids[shapely.has_z(geoms)] += 8
        unique_types, first_rows = np.unique(type_ids, return_index=True)
        for type_id, row in zip(unique_types.tolist(), first_rows.tolist()):
            type_rows.setdefault(type_id, offset + row)
        del geometry, geoms, bounds
        offset += rows
    if schema is None:
        runner.message("No schema")
        return None
    if any(e is not None and e[0] == 0 for e in extremes):
        # which signed zero numpy reports as the min/max depends on the order of the values
        runner.message("zero in extremes")
        return None
    wkb = pa.chunked_array(wkb_chunks, type=schema.field(geometry_column).type)
    del wkb_chunks
    if wkb.nbytes > WKB_CHUNK_LIMIT:
        # the original geometry column would have been split into multiple chunks, which changes the encoding
        runner.message("unsupported multiple chunks")
        return None

    # sort order
    cell_ids = assign_cell_ids(lon, lat, SEGMENT_S2_CELL_LEVEL)
    del lon, lat
    order = np.argsort(cell_ids, kind="stable") # same permutation as sort_index(kind="stable")

    def to_arrow(rows):
        arrays = [wkb.take(rows) if field.name == geometry_column else pa.array(columns[field.name][rows], type=field.type) for field in schema]
        gdf = _arrow_to_geopandas(pa.Table.from_arrays(arrays, schema=schema), geo_metadata)
        gdf.index = gpd.pd.Index(cell_ids[rows], name="cell_id")
        return _geopandas_to_arrow(gdf, index=True)

    # the schema (including the geo metadata) of the whole tile is the same as that of a subset of
    # rows containing every geometry type and each extreme of the bounding box
    representative_rows = np.array(sorted({0, *type_rows.values(), *(e[1] for e in extremes if e is not None)}))
    tile_schema = to_arrow(representative_rows).schema
    sorted_cell_ids = cell_ids[order]
    return tile_schema, sorted_cell_ids, lambda start, end: to_arrow(order[start:end])

# write optimized parquet
def write_optimized_parquet(schema, cell_ids, get_slice, output_file, target_bytes=ROW_GROUP_TARGET_SIZE_MB*1048576):
    """
    cell_ids are the sorted cell ids of every row, and get_slice(start, end) returns those rows as an arrow table
    rows per row group adapt to the bytes actually written, so each group lands near target_bytes
    row groups are extended to the end of its last cell_id so no cell spans two groups
    column chunks are written in schema order, so FIRST_COLUMNS are moved to the front
    """
    order = [name for name in FIRST_COLUMNS if name in schema.names] + [name for name in schema.names if name not in FIRST_COLUMNS]
    schema = pa.schema([schema.field(name) for name in order], metadata=schema.metadata)
    cell_idx = schema.get_field_index("cell_id")
    floats = [f.name for f in schema if pa.types.is_floating(f.type)]
    num_rows = len(cell_ids)
    with pa.OSFile(output_file, "wb") as sink:
        with pq.ParquetWriter(sink, schema,
                              sorting_columns=[pq.SortingColumn(cell_idx)],
                              compression="zstd",
                              # byte_stream_split only takes effect on columns without dictionary encoding
                              use_dictionary=[name for name in schema.names if name not in floats],
                              use_byte_stream_split=floats,
                              write_statistics=["cell_id", "time_ns"]) as writer:
            start, rows = 0, target_bytes // ROW_BYTES_ESTIMATE
            while start < num_rows:
                end = min(start + rows, num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(get_slice(start, end).select(order), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# ##############
# Main
# ##############

try:

    cell_id_str = partition.split("/")[-2]
    filenames = list_bucket(f"s3://{partition}")
    if len(filenames) == 0: raise RuntimeError("no files")
    built = build_streaming(filenames)
    if built is None:
        runner.message("using in-memory build")
        built = build_in_memory(filenames)
    schema, cell_ids, get_slice = built
    runner.message(f"{len(cell_ids)} rows from {len(filenames)} files")
    local_parquet_file = f"/tmp/ATL06T_{cell_id_str}.parquet"
    remote_parquet_file = f"{OUTPUT_PREFIX}/tiles/ATL06T_{cell_id_str}.parquet"
    write_optimized_parquet(schema, cell_ids, get_slice, local_parquet_file)
    s3.upload_file(local_parquet_file, OUTPUT_BUCKET, remote_parquet_file)
    runner.output(remote_parquet_file)
    runner.message(f"{len(cell_ids)} rows written for {cell_id_str}")
    os.remove(local_parquet_file)

except RuntimeError as e:

    runner.fail(f"Exception: {e}")

except Exception as e:

    runner.fail(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()