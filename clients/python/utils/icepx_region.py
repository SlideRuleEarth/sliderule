import sys
from datetime import date
from sliderule import ipxapi
from sliderule import icesat2
from utils import parse_command_line
import matplotlib.pyplot as plt
import icepyx

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    today = date.today()

    # set script defaults
    scfg = {
        "url": 'localhost',
        "organization": None,
        "asset": 'atlas-local'
    }

    # set icepx defaults
    icfg = {
        "short_name": 'ATL03',
        "spatial_extent": 'tests/data/grandmesa.shp',
        "date_range": ['2018-01-01', "{}-{}-{}".format(today.year, today.month, today.day)],
        "cycles": None,
        "tracks": None
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

    # create icepx region
    iregion = icepyx.Query(icfg["short_name"], icfg["spatial_extent"], icfg["date_range"], cycles=icfg["cycles"], tracks=icfg["tracks"])

    # visualize icepx region
    # iregion.visualize_spatial_extent()

    # display summary information
    iregion.product_summary_info()
    # print("Available Granules:", iregion.avail_granules())
    # print("Available Granule IDs:", iregion.avail_granules(ids=True))

    # initialize sliderule api
    icesat2.init(scfg["url"], verbose=True, organization=scfg["organization"])

    # generate sliderule atl06 elevations
    # parms["poly"] = icesat2.toregion(icfg["spatial_extent"])["poly"]
    atl06_sr = ipxapi.atl06p(iregion, parms, scfg["asset"])

    # create plot
    f, ax = plt.subplots()
    vmin, vmax = atl06_sr['h_mean'].quantile((0.02, 0.98))
    atl06_sr.plot(ax=ax, column='h_mean', cmap='inferno', s=0.1, vmin=vmin, vmax=vmax)
    plt.show()
