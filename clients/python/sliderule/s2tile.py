# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import uuid
import duckdb
import numpy as np
import pyarrow as pa
import pyarrow.fs as pafs
import pyarrow.dataset as ds
import geopandas as gpd
from concurrent.futures import Future, ThreadPoolExecutor
from threading import Lock
from sliderule import sliderule

###############################################################################
# GLOBALS
###############################################################################

TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
S3_REGION = "us-west-2"
TIME_UNIT_NS = {"s": 1_000_000_000, "ms": 1_000_000, "us": 1_000, "ns": 1}

# pre-buffer the selected column chunks, coalescing ranges separated by holes up to 1MB; reading a little
# extra data is far cheaper than issuing a separate S3 request for every column chunk when only some columns are read
PARQUET_FORMAT = ds.ParquetFileFormat(default_fragment_scan_options=ds.ParquetFragmentScanOptions(
    pre_buffer=True, cache_options=pa.CacheOptions(hole_size_limit=1 << 20, range_size_limit=64 << 20, lazy=False)))

###############################################################################
# CLASSES
###############################################################################

#
# S2 Tile Reader
#
class Reader:

    #
    # constructor
    #
    def __init__ (self, bucket, prefix, region=S3_REGION, anonymous=False, max_workers=8, tile_level=TILE_S2_CELL_LEVEL, segment_level=SEGMENT_S2_CELL_LEVEL):
        self.bucket = bucket
        self.prefix = prefix
        self.s3_fs = pafs.S3FileSystem(region=region, anonymous=anonymous)
        self.connection = duckdb.connect()
        try:
            # only go to the network to install the extension when it is not already available locally
            try:
                self.connection.sql("LOAD geography;")
            except duckdb.Error:
                self.connection.sql("INSTALL geography FROM community; LOAD geography;")
        except Exception:
            self.connection.close()
            raise
        self.max_workers = max_workers
        self.tile_level = tile_level
        self.segment_level = segment_level

    #
    # resource management
    #
    def close(self):
        self.connection.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    #
    # [lo, hi] range of cell ids covered by each cell (any level)
    #
    @staticmethod
    def cell_ranges(cells):
        ext = (cells & (~cells + np.uint64(1))) - np.uint64(1)
        return cells - ext, cells + ext

    #
    # mask of ids inside any of the sorted, disjoint [lo, hi] ranges
    #
    @staticmethod
    def in_ranges(ids, lo, hi):
        if len(lo) == 0:
            return np.zeros(len(ids), bool)
        k = np.searchsorted(lo, ids, side="right") - 1
        return (k >= 0) & (ids <= hi[np.maximum(k, 0)])

    #
    # split a tile into cells fully inside the aoi (mixed levels) and segment level cells crossing its edge
    #
    @staticmethod
    def classify_cells(cursor, poly_table, tile_id, tile_level=TILE_S2_CELL_LEVEL, segment_level=SEGMENT_S2_CELL_LEVEL):
        interior, cells = [], np.array([tile_id], dtype=np.uint64)
        for lvl in range(tile_level, segment_level + 1):
            if len(cells) == 0:
                break
            cursor.register("cells", gpd.pd.DataFrame({"c": cells}))
            # CASE evaluates the intersects test only for cells that are not already fully inside
            res = cursor.sql(f"""
                SELECT c, CASE WHEN s2_contains(p.g, cg) THEN 2 WHEN s2_intersects(p.g, cg) THEN 1 ELSE 0 END AS rel
                FROM (SELECT c, c::S2_CELL::GEOGRAPHY AS cg FROM cells), {poly_table} p
            """).fetchnumpy()
            interior.append(res["c"][res["rel"] == 2])
            cells = res["c"][res["rel"] == 1]
            if lvl < segment_level:
                lsb = cells & (~cells + np.uint64(1))
                cells = np.sort(np.concatenate([cells - lsb + (lsb >> np.uint64(2)) * np.uint64(2 * k + 1) for k in range(4)]))
        return np.sort(np.concatenate(interior)), cells

    #
    # leaf column index of a top level parquet column (differs from the arrow field index when nested columns are present)
    #
    @staticmethod
    def column_index(file_meta, name):
        for j in range(file_meta.num_columns):
            if file_meta.schema.column(j).path == name:
                return j
        raise KeyError(f"column {name} not found in parquet file")

    #
    # raw (physical int64) min/max statistics of a column per row group, and whether they are present
    #
    @staticmethod
    def column_bounds(file_meta, name):
        j = Reader.column_index(file_meta, name)
        n = file_meta.num_row_groups
        rg_min, rg_max, valid = np.zeros(n, np.int64), np.zeros(n, np.int64), np.zeros(n, bool)
        for i in range(n):
            stats = file_meta.row_group(i).column(j).statistics
            if stats is not None and stats.has_min_max:
                rg_min[i], rg_max[i], valid[i] = stats.min_raw, stats.max_raw, True
        return rg_min, rg_max, valid

    #
    # convert a utc time range to the raw int64 units of the time column
    #
    @staticmethod
    def raw_time_range(field_type, t0, t1):
        scale = TIME_UNIT_NS[field_type.unit] if pa.types.is_timestamp(field_type) else 1
        return -(-t0.value // scale), t1.value // scale

    #
    # select row groups whose cell_id min/max overlaps any of the sorted, disjoint [lo, hi] ranges
    # and, if provided, whose time min/max overlaps the raw [t0, t1] time range
    #
    @staticmethod
    def select_row_groups(lo, hi, file_meta, time_range=None, time_column="time_ns"):
        rg_min, rg_max, valid = Reader.column_bounds(file_meta, "cell_id")
        rg_min, rg_max = rg_min.view(np.uint64), rg_max.view(np.uint64)
        # first range ending at or after the row group's min; they overlap if that range starts before the row group's max
        k = np.searchsorted(hi, rg_min, side="left")
        hit = k < len(hi)
        hit[hit] = lo[k[hit]] <= rg_max[hit]
        # row groups without statistics cannot be ruled out
        hit |= ~valid
        if time_range is not None:
            t_min, t_max, t_valid = Reader.column_bounds(file_meta, time_column)
            hit &= ~t_valid | ((t_max >= time_range[0]) & (t_min <= time_range[1]))
        return np.flatnonzero(hit).tolist()

    #
    # convert a geoparquet arrow table (wkb encoded geometry) to a geodataframe
    #
    @staticmethod
    def to_geodataframe(table):
        geo = json.loads(table.schema.metadata[b"geo"])
        name = geo["primary_column"]
        column_meta = geo["columns"][name]
        encoding = column_meta.get("encoding", "WKB")
        if encoding.upper() != "WKB":
            raise ValueError(f"unsupported geometry encoding: {encoding}")
        # per the geoparquet spec a missing crs means OGC:CRS84 and a null crs means unknown
        crs = column_meta.get("crs", "OGC:CRS84")
        if isinstance(crs, dict):
            crs = json.dumps(crs)
        df = table.drop_columns([name]).to_pandas()
        geometry = gpd.GeoSeries.from_wkb(table.column(name).to_numpy(zero_copy_only=False), index=df.index, crs=crs)
        df.insert(min(table.column_names.index(name), len(df.columns)), name, geometry)
        return gpd.GeoDataFrame(df, geometry=name, crs=crs)

    #
    # exact point in polygon test
    #
    def points_inside(self, geometry, poly_table):
        ll = geometry.to_crs("EPSG:4326")
        with self.connection.cursor() as cursor:
            cursor.register("pts", gpd.pd.DataFrame({"lon": ll.x.to_numpy(), "lat": ll.y.to_numpy()}))
            return cursor.sql(
                f"SELECT s2_contains(p.g, s2_cellfromlonlat(lon, lat)::GEOGRAPHY) AS inside FROM pts, {poly_table} p"
            ).fetchnumpy()["inside"]

    #
    # cell id ranges of a tile: (interior ranges, boundary ranges, combined ranges)
    #
    def tile_ranges(self, cell, poly_table):
        # duckdb connections are not thread safe; a cursor is a separate connection to the same database
        with self.connection.cursor() as cursor:
            interior, boundary = Reader.classify_cells(cursor, poly_table, cell, self.tile_level, self.segment_level)
        return Reader.cell_ranges(interior), Reader.cell_ranges(boundary), Reader.cell_ranges(np.sort(np.concatenate([interior, boundary])))

    #
    # tile ranges are the same for every year, so compute them once per cell and share them across threads
    #
    def cached_tile_ranges(self, cache, lock, cell, poly_table):
        with lock:
            future = cache.get(cell)
            owner = future is None
            if owner:
                future = cache[cell] = Future()
        if owner:
            try:
                future.set_result(self.tile_ranges(cell, poly_table))
            except BaseException as e:
                future.set_exception(e)
        return future.result()

    #
    # read the rows of one tile inside the area of interest; returns (gdf or None, status)
    #
    def read_tile(self, tile_id, get_ranges, poly_table, t0, t1, enable_time_filter, exact, columns=None, time_column="time_ns"):
        year, cell = tile_id
        if year:
            tile_name = f"{self.bucket}/{self.prefix}/tiles/ATL06T_{year}_S{cell:016X}.parquet"
        else:
            tile_name = f"{self.bucket}/{self.prefix}/tiles/ATL06T_S{cell:016X}.parquet"
        try:
            fragment = PARQUET_FORMAT.make_fragment(tile_name, filesystem=self.s3_fs)
            metadata = fragment.metadata # reads the footer
        except FileNotFoundError:
            return None, "not found"
        except OSError as e:
            # e.g. a missing key read anonymously is reported as access denied; don't abort the remaining tiles
            return None, f"unable to open - {e}"
        schema = fragment.physical_schema
        (interior_lo, interior_hi), (boundary_lo, boundary_hi), (lo, hi) = get_ranges(cell)
        time_range = Reader.raw_time_range(schema.field(time_column).type, t0, t1) if enable_time_filter else None
        row_groups = Reader.select_row_groups(lo, hi, metadata, time_range, time_column)
        if not row_groups:
            return None, "empty"
        read_columns = None
        if columns is not None:
            missing = [c for c in columns if c not in schema.names]
            if missing:
                raise ValueError(f"columns not found in tile: {missing}")
            # the geometry, cell id, and (when filtering) time columns are needed to build and trim the result
            required = [json.loads(schema.metadata[b"geo"])["primary_column"], "cell_id", time_column]
            read_columns = list(dict.fromkeys([*columns, *required]))
        table = fragment.subset(row_group_ids=row_groups).to_table(columns=read_columns)
        # filter on the raw columns before paying for geometry decoding and pandas conversion
        ids = table.column("cell_id").to_numpy().view(np.uint64)
        interior = Reader.in_ranges(ids, interior_lo, interior_hi)
        boundary = Reader.in_ranges(ids, boundary_lo, boundary_hi)
        if time_range is not None:
            times = table.column(time_column).to_numpy().view(np.int64)
            in_time = (times >= time_range[0]) & (times <= time_range[1])
            interior &= in_time
            boundary &= in_time
        candidates = interior | boundary
        table = table.filter(pa.array(candidates))
        gdf = Reader.to_geodataframe(table)
        boundary = boundary[candidates]
        if exact and boundary.any():
            # only points in segment level cells crossing the polygon edge need an exact point-in-polygon test
            keep = ~boundary
            keep[boundary] = self.points_inside(gdf.geometry[boundary], poly_table)
            gdf = gdf[keep]
        return gdf, f"{len(gdf)} rows from {len(row_groups)}/{metadata.num_row_groups} row groups"

    #
    # read tiles; columns is an optional list of columns to return (default all), the geometry column is always included
    #
    def read_tiles(self, aoi, start=None, end=None, exact=True, verbose=False, columns=None):
        if isinstance(columns, str):
            columns = [columns]
        # get set of S2 cells that cover area of interest
        aoi_gdf = sliderule.toregion(aoi)["gdf"]
        if aoi_gdf.crs is not None:
            aoi_gdf = aoi_gdf.to_crs("EPSG:4326")
        wkt = aoi_gdf.union_all().wkt
        # a regular (not TEMP) table so the per-thread cursors can see it; uniquely named so concurrent calls don't collide
        poly_table = f"poly_geog_{uuid.uuid4().hex}"
        self.connection.execute(f"CREATE TABLE {poly_table} AS SELECT s2_prepare(s2_geogfromtext(?)) AS g", [wkt])
        try:
            cells = [row[0] for row in self.connection.execute(
                f"SELECT UNNEST(s2_covering_fixed_level(s2_geogfromtext(?), {self.tile_level}))::UBIGINT", [wkt]).fetchall()
            ]
            # get times
            enable_time_filter = (start is not None) or (end is not None)
            t0 = gpd.pd.to_datetime(start, utc=True) if start is not None else gpd.pd.to_datetime("2018-10-01", utc=True)
            t1 = gpd.pd.to_datetime(end, utc=True) if end is not None else gpd.pd.Timestamp.now(tz="UTC")
            if self.prefix == "atl06tv2":
                years = range(t0.year, t1.year + 1)
            else:
                years = [None]
            tiles = [(year, cell) for year in years for cell in cells]
            # read all tiles corresponding to an S2 cell in the area of interest
            cache, lock = {}, Lock()
            get_ranges = lambda cell: self.cached_tile_ranges(cache, lock, cell, poly_table)
            gdfs = []
            with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
                results = executor.map(
                    lambda tile_id: self.read_tile(tile_id, get_ranges, poly_table, t0, t1, enable_time_filter, exact, columns),
                    tiles)
                for i, ((year, cell), (gdf, status)) in enumerate(zip(tiles, results)):
                    if verbose: print(f"Read [{i+1}/{len(tiles)}] tile {year} {cell:016X} - {status}")
                    if gdf is not None:
                        gdfs.append(gdf)
        finally:
            self.connection.execute(f"DROP TABLE IF EXISTS {poly_table}")
        # combine tiles
        if len(gdfs) > 0:
            return gpd.GeoDataFrame(gpd.pd.concat(gdfs), crs=gdfs[0].crs)
        else:
            return None
