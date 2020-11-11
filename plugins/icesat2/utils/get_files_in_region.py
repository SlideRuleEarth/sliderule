#
# Imports
#
import sys
import json
from sliderule import icesat2

#
# Region of Interest (optionally overriden below from command line)
#
region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
           {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
           {"lon": -107.7818591266989, "lat": 39.26613714985466},
           {"lon": -108.3605610678553, "lat": 39.25086131372244},
           {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

#
# Dataset Name
#
dataset='ATL03'

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override region of interest
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as regionfile:
            region = json.load(regionfile)["region"]

    # Override dataset
    if len(sys.argv) > 2:
        dataset = sys.argv[2]

    # Query CMR for list of resources
    resources = icesat2.cmr(region, short_name=dataset)

    # Display Resources
    for resource in resources:
        print(resource)
