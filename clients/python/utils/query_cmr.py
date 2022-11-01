#
# Imports
#
import sys
from sliderule import icesat2
from utils import parse_command_line

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Defaults
    cfg = {
        "region": "examples/grandmesa.geojson",
        "tolerance": 0.0,
        "dataset": "ATL03"
    }

    # Command line parameters
    parse_command_line(sys.argv, cfg)

    # Override region of interest
    region = icesat2.toregion(cfg["region"], cfg["tolerance"])

    # Query CMR for list of resources
    resources = icesat2.cmr(polygon=region["poly"], short_name=cfg["dataset"])
    print("Region: {} points, {} files".format(len(region["poly"]), len(resources)))
    for resource in resources:
        print(resource)
