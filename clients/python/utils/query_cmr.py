#
# Imports
#
import sys
import sliderule
from utils import parse_command_line
from sliderule import earthdata

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Defaults
    cfg = {
        "region": "examples/grandmesa.geojson",
        "tolerance": 0.0,
        "dataset": "ATL03",
        "version": "005",
    }

    # Command line parameters
    parse_command_line(sys.argv, cfg)

    # Override region of interest
    region = sliderule.toregion(cfg["region"], cfg["tolerance"])

    # Query CMR for list of resources
    resources = earthdata.cmr(short_name=cfg["dataset"], version=cfg["version"], polygon=region["poly"])
    print("Region: {} points, {} files".format(len(region["poly"]), len(resources)))
    for resource in resources:
        print(resource)
