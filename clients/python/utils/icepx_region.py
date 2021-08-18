import sys
from datetime import date
from sliderule import ipxapi
from sliderule import icesat2
import icepyx

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

def parse_command_line(args, cfg):
    i = 1
    while i < len(args):
        for entry in cfg:
            if args[i] == '--'+entry:
                if type(cfg[entry]) is str:
                    cfg[entry] = args[i + 1]
                elif type(cfg[entry]) is list:
                    l = []
                    while (i + 1) < len(args) and args[i + 1].isnumeric():
                        l.append(int(args[i + 1]))
                        i += 1
                    cfg[entry] = l
                elif type(cfg[entry]) is int:
                    cfg[entry] = int(args[i + 1])
                i += 1
        i += 1
    return cfg

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    today = date.today()

    # set script defaults
    scfg = {
        "url": ['127.0.0.1'],
        "asset": 'atlas-local'
    }

    # set icepx defaults
    icfg = {
        "short_name": 'ATL03',
        "spatial_extent": 'valgrande.shp',
        "date_range": ['2018-01-01', "{}-{}-{}".format(today.year, today.month, today.day)],
        "cycles": [],
        "tracks": []
    }

    # set processing parameter defaults
    parms = {
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # get command line parameters
    parse_command_line(sys.argv, icfg)
    parse_command_line(sys.argv, scfg)
    parse_command_line(sys.argv, parms)

    # massage command line parameters
    if len(icfg["cycles"]) == 0:
        icfg["cycles"] = None
    if len(icfg["tracks"]) == 0:
        icfg["tracks"] = None

    # create icepx region
    iregion = icepyx.Query(icfg["short_name"], icfg["spatial_extent"], icfg["date_range"], cycles=icfg["cycles"], tracks=icfg["tracks"])

    # visualize icepx region
    iregion.visualize_spatial_extent()

    # display summary information
    iregion.product_summary_info()
    print("Available Granules:", iregion.avail_granules())
    print("Available Granule IDs:", iregion.avail_granules(ids=True))

    # initialize sliderule api
    icesat2.init(scfg["url"], verbose=True)

    # generate sliderule atl06 elevations
    parms["poly"] = icesat2.toregion(icfg["spatial_extent"])
    ipxapi.atl06p(iregion, parms, scfg["asset"])

