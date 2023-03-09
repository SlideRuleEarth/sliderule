#
# Imports
#
import sys
import json
import sliderule
from utils import parse_command_line
from sliderule import earthdata

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Defaults
    cfg = {
        "dataset": "HLS",
        "region": "examples/grandmesa.geojson",
        "time_start": "2021-01-01T00:00:00Z",
        "time_end": "2022-01-01T23:59:59Z",
        "display_all": False
    }

    # Command line parameters
    parse_command_line(sys.argv, cfg)

    # Region of interest
    polygon = sliderule.toregion(cfg["region"])["poly"]

    # Query CMR for list of resources
    geojson = earthdata.stac(short_name=cfg["dataset"], polygon=polygon, time_start=cfg["time_start"], time_end=cfg["time_end"])

    # Display Results
    print("Returned {} features".format(geojson["context"]["returned"]))
    if cfg["display_all"]:
        print(json.dumps(geojson, indent=2))

