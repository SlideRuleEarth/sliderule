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

# Imports
import sliderule
from sliderule import earthdata, icesat2
import argparse
import time
import os

# Globals
ATL03_GRANULE_GRANDMESA = "ATL03_20181017222812_02950102_005_01.h5"
ATL03_GRANULE_DICKSONFJORD = "ATL03_20190314093716_11600203_005_01.h5"

# Generate Region Polygon
region = sliderule.toregion("tests/data/grandmesa.geojson")

# Command Line Arguments
parser = argparse.ArgumentParser(description="""Subset granules""")
parser.add_argument('--benchmarks',     '-b',   nargs='+', type=str,    default=[])
parser.add_argument('--domain',         '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   '-o',   type=str,               default="sliderule")
parser.add_argument('--desired_nodes',  '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',   '-l',   type=int,               default=120)
parser.add_argument('--verbose',        '-v',   action='store_true',    default=False)
parser.add_argument('--loglvl',         '-j',   type=str,               default="CRITICAL")
parser.add_argument('--nocleanup',      '-u',   action='store_true',    default=False)
parser.add_argument('--no_aux',         '-x',   action='store_true',    default=False)
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# Configure SlideRule Client
if args.no_aux:
    sliderule.set_processing_flags(aux=False)

# ########################################################
# Utilities
# ########################################################

# Display Results
def display_results(name, gdf, duration):
    print(f'{name} <{len(gdf)} x {len(gdf.keys())}> - {duration:.6f} secs')

# ########################################################
# Benchmarks
# ########################################################

# ------------------------------------
# Benchmark ATL06 AIO
# ------------------------------------
def atl06_aoi():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
    }
    return icesat2.atl06p(parms)

# ------------------------------------
# Benchmark ATL06 Ancillary
# ------------------------------------
def atl06_ancillary():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "atl03_geo_fields": ["solar_elevation"]
    }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_GRANDMESA])

# ------------------------------------
# Benchmark ATL03 Ancillary
# ------------------------------------
def atl03_ancillary():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "atl03_ph_fields":  ["ph_id_count"]
    }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_GRANDMESA])

# ------------------------------------
# Benchmark ATL06 Parquet
# ------------------------------------
def atl06_parquet():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "cnf":              icesat2.CNF_SURFACE_LOW,
        "ats":              3.0,
        "cnt":              2,
        "len":              10.0,
        "res":              10.0,
        "output":           { "path": "testfile.parquet", "format": "parquet", "open_on_complete": True } }
    gdf = icesat2.atl06p(parms, resources=[ATL03_GRANULE_GRANDMESA])
    if not args.nocleanup:
        os.remove("testfile.parquet")
    return gdf

# ------------------------------------
# Benchmark ATL03 Parquet
# ------------------------------------
def atl03_parquet():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "cnf":              icesat2.CNF_SURFACE_LOW,
        "ats":              3.0,
        "cnt":              2,
        "len":              10.0,
        "res":              10.0,
        "output":           { "path": "testfile.parquet", "format": "parquet", "open_on_complete": True } }
    gdf = icesat2.atl03sp(parms, resources=[ATL03_GRANULE_GRANDMESA])
    if not args.nocleanup:
        os.remove("testfile.parquet")
    return gdf

# ------------------------------------
# Benchmark ATL06 Sample Landsat
# ------------------------------------
def atl06_sample_landsat():
    time_start = "2021-01-01T00:00:00Z"
    time_end = "2021-02-01T23:59:59Z"
    catalog = earthdata.stac(short_name="HLS", polygon=region['poly'], time_start=time_start, time_end=time_end, as_str=True)
    parms = {
        "poly": region['poly'],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_LOW,
        "ats": 20.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "samples": {"ndvi": {"asset": "landsat-hls", "use_poi_time": True, "catalog": catalog, "bands": ["NDVI"]}} }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_GRANDMESA])

# ------------------------------------
# Benchmark ATL06 Sample (Zonal) ArcticDEM
# ------------------------------------
def atl06_sample_zonal_arcticdem():
    parms = {
        "poly": sliderule.toregion("tests/data/dicksonfjord.geojson")['poly'],
        "cnf": "atl03_high",
        "ats": 5.0,
        "cnt": 5,
        "len": 20.0,
        "res": 10.0,
        "maxi": 1,
        "samples": {"mosaic": {"asset": "arcticdem-mosaic", "radius": 10, "zonal_stats": True}} }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_DICKSONFJORD])

# ------------------------------------
# Benchmark ATL06 Sample (Nearest Neighbor) ArcticDEM
# ------------------------------------
def atl06_sample_nn_arcticdem():
    parms = {
        "poly": sliderule.toregion("tests/data/dicksonfjord.geojson")['poly'],
        "cnf": "atl03_high",
        "ats": 5.0,
        "cnt": 5,
        "len": 20.0,
        "res": 10.0,
        "maxi": 1,
        "samples": {"mosaic": {"asset": "arcticdem-mosaic"}} }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_DICKSONFJORD])

# ------------------------------------
# Benchmark ATL06 Multi-Sample (Nearest Neighbor) ArcticDEM
# ------------------------------------
def atl06_msample_nn_arcticdem():
    parms = {
        "poly": sliderule.toregion("tests/data/dicksonfjord.geojson")['poly'],
        "cnf": "atl03_high",
        "ats": 5.0,
        "cnt": 5,
        "len": 20.0,
        "res": 10.0,
        "maxi": 1,
        "samples": {"mosaic": {"asset": "arcticdem-mosaic"}} }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_DICKSONFJORD])

# ------------------------------------
# Benchmark ATL06 No Sampling ArcticDEM
# ------------------------------------
def atl06_no_sample_arcticdem():
    parms = {
        "poly": sliderule.toregion("tests/data/dicksonfjord.geojson")['poly'],
        "cnf": "atl03_high",
        "ats": 5.0,
        "cnt": 5,
        "len": 20.0,
        "res": 10.0,
        "maxi": 1 }
    return icesat2.atl06p(parms, resources=[ATL03_GRANULE_DICKSONFJORD])

# ------------------------------------
# Benchmark ATL03 Rasterized Subset
# ------------------------------------
def atl03_rasterized_subset():
    parms = {
        "poly": region['poly'],
        "region_mask": region['raster'],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_LOW,
        "ats": 20.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0 }
    return icesat2.atl03sp(parms, resources=[ATL03_GRANULE_GRANDMESA])

# ------------------------------------
# Benchmark ATL03 Polygon Subset
# ------------------------------------
def atl03_polygon_subset():
    parms = {
        "poly": region['poly'],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_LOW,
        "ats": 20.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0 }
    return icesat2.atl03sp(parms, resources=[ATL03_GRANULE_GRANDMESA])

# ########################################################
# Main
# ########################################################

if __name__ == '__main__':

    # define benchmarks
    benchmarks = {
        "atl06_aoi":                    atl06_aoi,
        "atl06_ancillary":              atl06_ancillary,
        "atl03_ancillary":              atl03_ancillary,
        "atl06_parquet":                atl06_parquet,
        "atl03_parquet":                atl03_parquet,
        "atl06_sample_landsat":         atl06_sample_landsat,
        "atl06_sample_zonal_arcticdem": atl06_sample_zonal_arcticdem,
        "atl06_sample_nn_arcticdem":    atl06_sample_nn_arcticdem,
        "atl06_msample_nn_arcticdem":   atl06_msample_nn_arcticdem,
        "atl06_no_sample_arcticdem":    atl06_no_sample_arcticdem,
        "atl03_rasterized_subset":      atl03_rasterized_subset,
        "atl03_polygon_subset":         atl03_polygon_subset,
    }

    # build list of benchmarks to run
    benchmarks_to_run = args.benchmarks
    if len(benchmarks_to_run) == 0:
        benchmarks_to_run = benchmarks.keys()

    # run benchmarks
    for benchmark in benchmarks_to_run:
        tstart = time.perf_counter()
        gdf = benchmarks[benchmark]()
        display_results(benchmark, gdf, time.perf_counter() - tstart)
