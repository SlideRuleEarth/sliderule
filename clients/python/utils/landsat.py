#
# HLS LANDSAT test
from pystac_client import Client

def BuildSquare(lon, lat, delta):
    c1 = [lon + delta, lat + delta]
    c2 = [lon + delta, lat - delta]
    c3 = [lon - delta, lat - delta]
    c4 = [lon - delta, lat + delta]

    # This order matters for query to use 'inside of polygon' area
    geometry = {"type": "Polygon", "coordinates": [[ c1, c4, c3, c2, c1 ]]}
    return geometry


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    catalog = Client.open("https://cmr.earthdata.nasa.gov/stac/LPCLOUD")

    timeRange = '2021-01-01/2022-01-01'
    geometry  = BuildSquare(-178, 50, 1)
    mybbox    = [-179,41,-177,51]

    print("Searching with bbox...")

    results = catalog.search(collections=['HLSS30.v2.0'],
                             bbox = mybbox,
                             datetime=timeRange)
    print(f"HLSS30: {results.matched()} items found")

    results = catalog.search(collections=['HLSL30.v2.0'],
                             bbox = mybbox,
                             datetime=timeRange)
    print(f"HLSL30: {results.matched()} items found")

    results = catalog.search(collections=['HLSS30.v2.0', 'HLSL30.v2.0'],
                             bbox = mybbox,
                             datetime=timeRange)
    print(f"Results matched: {results.matched()}")


    print("Searching with Intersects...")
    results = catalog.search(collections=['HLSS30.v2.0', 'HLSL30.v2.0'],
                             datetime=timeRange,
                             intersects=geometry)

    # print(f"{results.url_with_parameters()}")
    print(f"Results matched: {results.matched()}")
    items = [i.to_dict() for i in results.get_items()]
    print(f"Items fetched:   {len(items)}")


    print("Done!")
