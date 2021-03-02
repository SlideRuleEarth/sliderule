# python

import sys
import json
import sliderule

atl03_asset = "atlas-local"
index = "atl03.index"

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    resource_list_file = sys.argv[1]
    url_parm = 2
    asset_parm = 3
    index_parm = 4

    # Override server URL from command line
    if len(sys.argv) > url_parm:
        sliderule.set_url(sys.argv[url_parm])

    # Override asset from command line
    if len(sys.argv) > asset_parm:
        atl03_asset = sys.argv[asset_parm]

    # Override index from command line
    if len(sys.argv) > index_parm:
        index = sys.argv[index_parm]

    # Set Verbosity #
    sliderule.set_verbose(True)

    # Read Resource List
    resources = []
    f = open(resource_list_file, "r")
    for resource in f.readlines():
        resources.append(resource.strip())
    f.close()

    # Build Index
    rqst = { "atl03-asset": atl03_asset, "resources": resources }
    rsps = sliderule.engine("indexer", rqst)
    print(len(rsps))
    print(rsps)
    f = open(index, "w")
    f.write('name,t0,t1,lat0,lon0,lat1,lon1,cycle,rgt\n')
    for rsp in rsps:
        print(rsp["name"])
        f.write('%s,%f,%f,%f,%f,%f,%f,%d,%d\n' % (rsp['name'], rsp['t0'], rsp['t1'], rsp['lat0'], rsp['lon0'], rsp['lat1'], rsp['lon1'], rsp['cycle'], rsp['rgt']))
    f.close()
