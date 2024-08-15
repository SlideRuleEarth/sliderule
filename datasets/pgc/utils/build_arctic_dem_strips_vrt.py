#
# Build index file (catalog) of ArcticDem hosted on AWS
import os
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

    collection_stac = 'https://pgc-opendata-dems.s3.us-west-2.amazonaws.com/arcticdem/strips/s2s041/2m.json'
    col = pystac.read_file(collection_stac)

    item_list = []
    vrt_list_files = []

    cnt = 0

    for link in col.links:
        if link.rel == pystac.RelType.CHILD:
            subcat = pystac.read_file(link.target)
            for _link in subcat.links:
                if _link.rel == pystac.RelType.CHILD:
                    item = pystac.read_file(_link)
                    dem = item.to_dict()['assets']['dem']['href']
                    dem = dem.replace(".", "", 1)
                    path = link.target.replace("https:/", "/vsis3")
                    path = path.replace(".s3.us-west-2.amazonaws.com", "", 1)
                    path = path.replace(".json", dem)
                    item_list.append(path)

            # print(f"Number of features: {len(item_list)}")

            scene_dem_list = path.replace("/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/", "", 1)
            latlon = scene_dem_list.split("/")[0]
            scene_dem_list = "/data/ArcticDem/strips/" + latlon + ".txt"

            vrt_list_files.append(scene_dem_list)

            with open(scene_dem_list, 'w') as f:
                for line in item_list:
                    f.write(f"{line}\n")

            print(f"Generated: {scene_dem_list}")

            # if cnt == 1:
            #     break
            # cnt += 1

    print(f"Generated: {len(vrt_list_files)} vrt list files")
    print("Building vrts now")

    for file in vrt_list_files:
        vrtfile = file.replace("txt", "vrt", 1)
        cmd = "gdalbuildvrt -input_file_list " + file + " " + vrtfile
        print(f"{cmd}")
        os.system(cmd)