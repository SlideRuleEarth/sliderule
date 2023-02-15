#
# HLS LANDSAT test
from pystac_client import Client

def BuildSquare(lon, lat, delta):
    c1 = [lon + delta, lat + delta]
    c2 = [lon + delta, lat - delta]
    c3 = [lon - delta, lat - delta]
    c4 = [lon - delta, lat + delta]
    geometry = {"type": "Polygon", "coordinates": [[ c1, c2, c3, c4, c1 ]]}
    return geometry


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    catalog = Client.open("https://landsatlook.usgs.gov/stac-server")

    timeRange = '2019-06-01/2021-06-01'
    geometry = BuildSquare(-59.346271, -34.233076, 0.04)
    # geometry  = BuildSquare(-178, 50, 1)
    mybbox    = [-179,41,-177,51]

    # print("Searching with Intersects...")
    # results = catalog.search(collections=['HLSS30.v2.0', 'HLSL30.v2.0'],
    #                          datetime=timeRange,
    #                          intersects=geometry)
    # print(f"{results.url_with_parameters()}")
    # print(f"{results.matched()} items found")

    results = catalog.search (
                             intersects = geometry,
                             datetime = timeRange,
                             query =  ['eo:cloud_cover95'],
                             collections = ["landsat-c2l2-sr"] )

    Landsat_items = [i.to_dict() for i in results.get_items()]
    print(f"{len(Landsat_items)} Landsat scenes fetched")

    print("Done!")
