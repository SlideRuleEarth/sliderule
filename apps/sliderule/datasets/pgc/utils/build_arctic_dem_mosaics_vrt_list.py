#
# Build index file list and create vrt
import geopandas as gpd
import numpy as np
import pandas as pd
import pystac


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # catalog_stac = 'https://pgc-opendata-dems.s3.us-west-2.amazonaws.com/pgc-data-stac.json'
    # cat = pystac.read_file(catalog_stac)
    # arcticdem_collection = cat.get_child('arcticdem')
    # mosaic_collection = arcticdem_collection.get_child('arcticdem-mosaics')

    collection_stac = 'https://pgc-opendata-dems.s3.us-west-2.amazonaws.com/arcticdem/mosaics/v3.0/2m.json'
    col = pystac.read_file(collection_stac)

    item_list = []
    cnt = 0

    for link in col.links:
        if link.rel == pystac.RelType.CHILD:
            subcat = pystac.read_file(link.target)
            print(subcat)
            # if cnt == 2:
            #     break
            # cnt += 1

            for _link in subcat.links:
                if _link.rel == pystac.RelType.CHILD:
                    item = pystac.read_file(_link.target)
                    dem = item.to_dict()['assets']['dem']['href']
                    dem = dem.replace(".", "", 1)
                    path = link.target.replace("https:/", "/vsis3")
                    path = path.replace(".s3.us-west-2.amazonaws.com", "", 1)
                    path = path.replace(".json", dem)
                    item_list.append(path)

    print(f"Number of features: {len(item_list)}")

    with open('/data/ArcticDem/mosaic_vrt_list.txt', 'w') as f:
        for line in item_list:
            f.write(f"{line}\n")

