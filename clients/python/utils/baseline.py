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
from sliderule import earthdata, icesat2, gedi
import argparse
import math
import numpy

# Command Line Arguments
parser = argparse.ArgumentParser(description="""Subset granules""")
parser.add_argument('--benchmarks',     '-b',   nargs='+', type=str,    default=[])
parser.add_argument('--domain',         '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   '-o',   type=str,               default="sliderule")
parser.add_argument('--desired_nodes',  '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',   '-l',   type=int,               default=120)
parser.add_argument('--verbose',        '-v',   action='store_true',    default=False)
parser.add_argument('--loglvl',         '-j',   type=str,               default="CRITICAL")
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# GEDI / 3DEP
def gedi_3dep():
    resource = "GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5"
    region = [  {"lon": -108.3435200747503, "lat": 38.89102961045247},
                {"lon": -107.7677425431139, "lat": 38.90611184543033},
                {"lon": -107.7818591266989, "lat": 39.26613714985466},
                {"lon": -108.3605610678553, "lat": 39.25086131372244},
                {"lon": -108.3435200747503, "lat": 38.89102961045247}  ]
    parms = {
        "poly": region,
        "degrade_filter": True,
        "l2_quality_filter": True,
        "beams": 0,
        "samples": {"3dep": {"asset": "usgs3dep-1meter-dem"}}
    }
    gdf = gedi.gedi04ap(parms, resources=[resource], as_numpy_array=True)
    samples = [values[0] for values in gdf["3dep.value"] if (type(values) == numpy.ndarray and not math.isnan(values[0]))]
    return sum(samples)/len(samples) # average value

# ICESat-2 / ArcticDEM
def icesat2_arcticdem():
    resource = "ATL03_20190314093716_11600203_005_01.h5"
    region = sliderule.toregion("tests/data/dicksonfjord.geojson")
    parms = { "poly": region['poly'],
              "cnf": "atl03_high",
              "srt": 3,
              "ats": 20.0,
              "cnt": 10,
              "len": 40.0,
              "res": 20.0,
              "maxi": 1,
              "samples": {"mosaic": {"asset": "arcticdem-mosaic"}} }
    gdf = icesat2.atl06p(parms, resources=[resource])
    return gdf["mosaic.value"].describe()["mean"]

# ICESat-2 / ATL06p
def icesat2_atl06p():
    resource = "ATL03_20181019065445_03150111_005_01.h5"
    parms = { "cnf": "atl03_high",
                "srt": 3,
                "ats": 20.0,
                "cnt": 10,
                "len": 40.0,
                "res": 20.0,
                "maxi": 1 }
    gdf = icesat2.atl06p(parms, resources=[resource])
    return gdf["h_mean"].mean()

# ICESat-2 / PhoREAL
def icesat2_phoreal():
    parms = {
        "poly": sliderule.toregion('tests/data/grandmesa.geojson')['poly'],
        "t0": '2019-11-14T00:00:00Z',
        "t1": '2019-11-15T00:00:00Z',
        "srt": icesat2.SRT_LAND,
        "len": 100,
        "res": 100,
        "pass_invalid": True, 
        "atl08_class": ["atl08_ground", "atl08_canopy", "atl08_top_of_canopy"],
        "phoreal": {"binsize": 1.0, "geoloc": "center"}
    }
    gdf = sliderule.run("atl03x", parms)
    return gdf["h_mean_canopy"].mean()
# 
# Main
#
print(f'GEDI / 3DEP = {gedi_3dep()}')
print(f'ICESat-2 / ArcticDEM = {icesat2_arcticdem()}')
print(f'ICESat-2 / ATL06p = {icesat2_atl06p()}')
print(f'ICESat-2 / PhoREAL = {icesat2_phoreal()}')