#
# Perform a proxy request for atl06-sr elevations
#

import sys
import logging
import time
from sliderule import icesat2
from utils import parse_command_line

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set Script Defaults
    cfg = {
        "url":                          'localhost',
        "organization":                 None,
        "asset":                        'atlas-local',
        "region":                       'examples/grandmesa.geojson',
        "resource":                     'ATL03_20181017222812_02950102_005_01.h5',
        "srt":                          icesat2.SRT_LAND,
        "cnf":                          icesat2.CNF_SURFACE_HIGH,
        "ats":                          10.0,
        "cnt":                          10,
        "len":                          40.0,
        "res":                          20.0,
        "maxi":                         1,
        "atl03_geolocation_fields":     [],
        "atl03_geocorrection_fields":   [],
        "atl03_height_fields":          [],
        "atl08_signal_photon_fields":   [],
        "profile":                      True
    }

    # Parse Configuration Parameters
    parse_command_line(sys.argv, cfg)

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)

    # Region of Interest #
    region = icesat2.toregion(cfg["region"])

    # Configure SlideRule #
    icesat2.init(cfg["url"], True, organization=cfg["organization"])

    # Build ATL06 Request #
    parms = {
        "poly":                         region['poly'],
        "raster":                       region['raster'],
        "srt":                          cfg['srt'],
        "cnf":                          cfg['cnf'],
        "ats":                          cfg['ats'],
        "cnt":                          cfg['cnt'],
        "len":                          cfg['len'],
        "res":                          cfg['res'],
        "maxi":                         cfg['maxi'],
        "atl03_geolocation_fields":     cfg['atl03_geolocation_fields'],
        "atl03_geocorrection_fields":   cfg['atl03_geocorrection_fields'],
        "atl03_height_fields":          cfg['atl03_height_fields'],
        "atl08_signal_photon_fields":   cfg['atl08_signal_photon_fields']
    }

    # Request ATL06 Data
    perf_start = time.perf_counter()
    gdf = icesat2.atl06p(parms, asset=cfg["asset"], resources=[cfg["resource"]])
    perf_stop = time.perf_counter()

    # Display Statistics
    num_elevations = len(gdf)
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if num_elevations > 0:
        print("Reference Ground Tracks: {}".format(gdf["rgt"].unique()))
        print("Cycles: {}".format(gdf["cycle"].unique()))
        print("Received {} elevations".format(num_elevations))
    else:
        print("No elevations were returned")

    # Display Profile
    print("\nTiming Profiles")
    for key in icesat2.profiles:
        print("{:16}: {:.6f} secs".format(key, icesat2.profiles[key]))