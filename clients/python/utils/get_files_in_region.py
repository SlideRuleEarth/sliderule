#
# Imports
#
import sys
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set Filename
    filename = sys.argv[1]

    # Override Tolerance
    tolerance = 0.0
    if len(sys.argv) > 2:
        tolerance = float(sys.argv[2])

    # Override dataset
    dataset='ATL03'
    if len(sys.argv) > 3:
        dataset = sys.argv[3]

    # Override region of interest
    regions = icesat2.toregion(filename, tolerance)

    # Query CMR for list of resources
    for i in range(len(regions)):
        region = regions[i]
        resources = icesat2.cmr(region, short_name=dataset)
        print("Region {}: {} points, {} files".format(i, len(region), len(resources)))
        for resource in resources:
            print(resource)
