#
# Test for landsat stac server
import geopandas as gpd
import numpy as np
import pandas as pd
import pystac
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

    stacServer  = "https://landsatlook.usgs.gov/stac-server"
    LandsatSTAC = Client.open(stacServer, headers=[])

    for collection in LandsatSTAC.get_collections():
        print(collection)

    geometry  = BuildSquare(-59.346271, -34.233076, 0.04)
    timeRange = '2019-06-01/2021-06-01'

    LandsatSearch = LandsatSTAC.search (
        intersects = geometry,
        datetime = timeRange,
        query =  ['eo:cloud_cover95'],
        collections = ["landsat-c2l2-sr"] )

    Landsat_items = [i.to_dict() for i in LandsatSearch.get_items()]
    print(f"{len(Landsat_items)} Landsat scenes fetched")

    for item in Landsat_items:
        red_href = item['assets']['red']['href']
        red_s3 = item['assets']['red']['alternate']['s3']['href']
        # print(red_href)
        print(red_s3)
