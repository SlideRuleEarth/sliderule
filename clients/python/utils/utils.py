import time
import json
import sliderule
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
                    if args[i + 1] == "None":
                        cfg[entry] = None
                    else:
                        cfg[entry] = args[i + 1]
                elif type(cfg[entry]) is list:
                    if args[i + 1] == "None":
                        cfg[entry] = None
                    else:
                        l = []
                        while (i + 1) < len(args) and '--' not in args[i + 1]:
                            if args[i + 1].isnumeric():
                                l.append(int(args[i + 1]))
                            else:
                                l.append(args[i + 1])
                            i += 1
                        cfg[entry] = l
                elif type(cfg[entry]) is int:
                    if args[i + 1] == "None":
                        cfg[entry] = None
                    else:
                        cfg[entry] = int(args[i + 1])
                elif type(cfg[entry]) is bool:
                    if args[i + 1] == "None":
                        cfg[entry] = None
                    elif args[i + 1] == "True" or args[i + 1] == "true":
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
        "domain":                   'localhost',
        "organization":             None,
        "asset":                    'atlas-local',
        "region":                   'examples/grandmesa.geojson',
        "resource":                 'ATL03_20181017222812_02950102_005_01.h5',
        "raster":                   True,
        "atl08_class":              [],
        "yapc.score":               0,
        "yapc.knn":                 0,
        "yapc.min_knn":             5,
        "yapc.win_h":               6.0,
        "yapc.win_x":               15.0,
        "yapc.version":             0,
        "srt":                      icesat2.SRT_LAND,
        "cnf":                      icesat2.CNF_SURFACE_HIGH,
        "ats":                      10.0,
        "cnt":                      10,
        "len":                      40.0,
        "res":                      20.0,
        "maxi":                     1,
        "atl03_geo_fields":         [],
        "atl03_ph_fields":          [],
        "samples":                  [],
        "profile":                  True,
        "verbose":                  True,
        "timeout":                  0,
        "rqst-timeout":             0,
        "node-timeout":             0,
        "read-timeout":             0,
        "output.path":              None,
        "output.format":            "native",
        "output.open_on_complete":  False
    }

    # Parse Configuration Parameters
    parse_command_line(args, cfg)

    # Configure SlideRule
    icesat2.init(cfg["domain"], cfg["verbose"], organization=cfg["organization"])

    # Build Initial Parameters
    parms = {
        "srt":  cfg['srt'],
        "cnf":  cfg['cnf'],
        "ats":  cfg['ats'],
        "cnt":  cfg['cnt'],
        "len":  cfg['len'],
        "res":  cfg['res'],
        "maxi": cfg['maxi'],
    }

    # Region of Interest
    if cfg["region"]:
        region = icesat2.toregion(cfg["region"])
        parms["poly"] = region['poly']
        if cfg["raster"]:
            parms["raster"] = region['raster']

    # Add Ancillary Fields
    if len(cfg['atl03_geo_fields']) > 0:
        parms['atl03_geo_fields'] = cfg['atl03_geo_fields']
    if len(cfg['atl03_ph_fields']) > 0:
        parms['atl03_ph_fields'] = cfg['atl03_ph_fields']

    # Add Rasters to Sample
    if len(cfg['samples']) > 0:
        parms['samples'] = cfg['samples']

    # Add ATL08 Classification
    if len(cfg['atl08_class']) > 0:
        parms['atl08_class'] = cfg['atl08_class']

    # Add YAPC Parameters
    if cfg["yapc.version"] > 0:
        parms["yapc"] = { "score": cfg["yapc.score"],
                          "knn": cfg["yapc.knn"],
                          "min_knn": cfg["yapc.min_knn"],
                          "win_h": cfg["yapc.win_h"],
                          "win_x": cfg["yapc.win_x"],
                          "version": cfg["yapc.version"] }

    # Provide Timeouts
    if cfg["timeout"] > 0:
        parms["timeout"] = cfg["timeout"]
        parms["rqst-timeout"] = cfg["timeout"]
        parms["node-timeout"] = cfg["timeout"]
        parms["read-timeout"] = cfg["timeout"]
    if cfg["rqst-timeout"] > 0:
        parms["rqst-timeout"] = cfg["rqst-timeout"]
    if cfg["node-timeout"] > 0:
        parms["node-timeout"] = cfg["node-timeout"]
    if cfg["read-timeout"] > 0:
        parms["read-timeout"] = cfg["read-timeout"]

    # Add Output Options
    if cfg["output.path"]:
        parms["output"] = { "path": cfg["output.path"],
                            "format": cfg["output.format"],
                            "open_on_complete": cfg["output.open_on_complete"] }
    # Latch Start Time
    tstart = time.perf_counter()

    # Return Parameters and Configuration
    return parms, cfg

#
# Display Timing
#
def display_timing():

    print("\nSlideRule Timing Profiles")
    for key in sliderule.profiles:
        print("{:20} {:.6f} secs".format(key + ":", sliderule.profiles[key]))

    print("\nICESat2 Timing Profiles")
    for key in icesat2.profiles:
        print("{:20} {:.6f} secs".format(key + ":", icesat2.profiles[key]))


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

    display_timing()

#
# Pretty Print JSON
#
def pprint(obj):
    print(json.dumps(obj, indent=2))
