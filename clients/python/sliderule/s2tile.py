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

import os
import json
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

###############################################################################
# GLOBALS
###############################################################################

TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
ROW_GROUP_TARGET_SIZE_MB = 2 # MB
S3_REGION = "us-west-2"

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
        self.connection.sql("INSTALL geography FROM community; LOAD geography;")
        self.max_workers = max_workers
        self.tile_level = tile_level
        self.segment_level = segment_level

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

    #
    # select row groups whose cell_id min/max overlaps any of the sorted, disjoint [lo, hi] ranges
    #
    @staticmethod
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

    #
    # trim geodataframe to area of interest
    #
    @staticmethod
    def trim_to_aoi(gdf, cursor, interior, boundary, exact=True):
        ids = gdf.index.to_numpy()
        keep = Reader.in_ranges(ids, *Reader.cell_ranges(interior))
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

    #
    # filter geodataframe with time range
    #
    @staticmethod
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

    #
    # read the rows of one tile inside the area of interest; returns (gdf or None, status)
    #
    def read_tile(self, tile_id, start, end, exact):
        try:
            # pre_buffer coalesces the selected column chunks into a few concurrent range requests
            pf = pq.ParquetFile(f"{self.bucket}/{self.prefix}/tiles/ATL06T_S{tile_id:016X}.parquet", filesystem=self.s3_fs, pre_buffer=True)
        except FileNotFoundError:
            return None, "not found"
        # duckdb connections are not thread safe; a cursor is a separate connection to the same database
        cursor = self.connection.cursor()
        try:
            interior, boundary = Reader.classify_cells(cursor, tile_id, self.tile_level, self.segment_level)
            lo, hi = Reader.cell_ranges(np.sort(np.concatenate([interior, boundary])))
            row_groups = Reader.select_row_groups(lo, hi, pf.metadata)
            if not row_groups:
                return None, "empty"
            gdf = _arrow_to_geopandas(pf.read_row_groups(row_groups, use_pandas_metadata=True))
            gdf = Reader.filter_time(gdf, start, end)
            gdf = Reader.trim_to_aoi(gdf, cursor, interior, boundary, exact)
            return gdf, f"{len(gdf)} rows from {len(row_groups)}/{pf.metadata.num_row_groups} row groups"
        finally:
            cursor.close()
            pf.close()

    #
    # read tiles
    #
    def read_tiles(self, aoi, start=None, end=None, exact=True, verbose=False):
        # get set of S2 cells that cover area of interest
        aoi_gdf = sliderule.toregion(aoi)["gdf"]
        if aoi_gdf.crs is not None:
            aoi_gdf = aoi_gdf.to_crs("EPSG:4326")
        wkt = aoi_gdf.union_all().wkt
        # a regular (not TEMP) table so the per-thread cursors can see it
        self.connection.execute("CREATE OR REPLACE TABLE poly_geog AS SELECT s2_prepare(s2_geogfromtext(?)) AS g", [wkt])
        tile_cover_ids = [row[0] for row in self.connection.execute(
            f"SELECT UNNEST(s2_covering_fixed_level(s2_geogfromtext(?), {self.tile_level}))::UBIGINT", [wkt]).fetchall()
        ]
        # read all tiles corresponding to an S2 cell in the area of interest
        gdfs = []
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            results = executor.map(
                lambda tile_id: self.read_tile(tile_id, start, end, exact),
                tile_cover_ids)
            for i, (tile_id, (gdf, status)) in enumerate(zip(tile_cover_ids, results)):
                if verbose: print(f"Read [{i+1}/{len(tile_cover_ids)}] tile {tile_id:016X} - {status}")
                if gdf is not None:
                    gdfs.append(gdf)
        # combine tiles
        if len(gdfs) > 0:
            return gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
        else:
            return None
