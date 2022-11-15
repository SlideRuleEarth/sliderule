import sys
import time
from sliderule import icesat2

#
# Globals
#
tstart = 0.0

#
# Parse Command Line
#
def parse_command_line(args, cfg):

    i = 1
    for i in range(1,len(args)):
        for entry in cfg:
            if args[i] == '--'+entry:
                if type(cfg[entry]) is str or cfg[entry] == None:
                    cfg[entry] = args[i + 1]
                elif type(cfg[entry]) is list:
                    l = []
                    while (i + 1) < len(args) and '--' not in args[i + 1]:
                        if args[i + 1].isnumeric():
                            l.append(int(args[i + 1]))
                        else:
                            l.append(args[i + 1])
                        i += 1
                    cfg[entry] = l
                elif type(cfg[entry]) is int:
                    cfg[entry] = int(args[i + 1])
                elif type(cfg[entry]) is bool:
                    if args[i + 1] == "True" or args[i + 1] == "true":
                        cfg[entry] = True
                    elif args[i + 1] == "False" or args[i + 1] == "false":
                        cfg[entry] = False

#
# Initialize Client
#
def initialize_client(args):

    global tstart

    # Set Script Defaults
    cfg = {
        "url":                  'localhost',
        "organization":         None,
        "asset":                'atlas-local',
        "region":               'examples/grandmesa.geojson',
        "resource":             'ATL03_20181017222812_02950102_005_01.h5',
        "raster":               True,
        "atl08_class":          [],
        "srt":                  icesat2.SRT_LAND,
        "cnf":                  icesat2.CNF_SURFACE_HIGH,
        "ats":                  10.0,
        "cnt":                  10,
        "len":                  40.0,
        "res":                  20.0,
        "maxi":                 1,
        "atl03_geo_fields":     [],
        "atl03_ph_fields":      [],
        "profile":              True,
        "verbose":              True
    }

    # Parse Configuration Parameters
    parse_command_line(args, cfg)

    # Region of Interest
    region = icesat2.toregion(cfg["region"])

    # Configure SlideRule
    icesat2.init(cfg["url"], cfg["verbose"], organization=cfg["organization"])

    # Build Initial Parameters
    parms = {
        "poly": region['poly'],
        "srt":  cfg['srt'],
        "cnf":  cfg['cnf'],
        "ats":  cfg['ats'],
        "cnt":  cfg['cnt'],
        "len":  cfg['len'],
        "res":  cfg['res'],
        "maxi": cfg['maxi'],
    }

    # Add Raster
    if cfg["raster"]:
        parms["raster"] = region['raster']

    # Add Ancillary Fields
    if len(cfg['atl03_geo_fields']) > 0:
        parms['atl03_geo_fields'] = cfg['atl03_geo_fields']
    if len(cfg['atl03_ph_fields']) > 0:
        parms['atl03_ph_fields'] = cfg['atl03_ph_fields']

    # Add ATL08 Classification
    if len(cfg['atl08_class']) > 0:
        parms['atl08_class'] = cfg['atl08_class']

    # Latch Start Time
    tstart = time.perf_counter()

    # Return Parameters and Configuration
    return parms, cfg

#
# Display Statistics
#
def display_statistics(gdf, name):

    global tstart

    perf_duration = time.perf_counter() - tstart
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if len(gdf) > 0:
        print("Reference Ground Tracks: {}".format(gdf["rgt"].unique()))
        print("Cycles: {}".format(gdf["cycle"].unique()))
        print("Received {} {}".format(len(gdf), name))
    else:
        print("No {} were returned".format(name))
        sys.exit(0)

    print("\nTiming Profiles")
    for key in icesat2.profiles:
        print("{:16}: {:.6f} secs".format(key, icesat2.profiles[key]))