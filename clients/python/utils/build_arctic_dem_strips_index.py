#
# Build index file (catalog) of ArcticDem hosted on AWS
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

    catalog_links = []
    collection_links = []
    item_list = []

    for link in col.links:
        if link.rel == pystac.RelType.CHILD:
            collection_links.append(link)

    print(f"Number of collection links: {len(collection_links)}")
    cnt = 0

    for link in collection_links:
        cnt += 1
        if cnt % 100 == 0:
            print(f"working collection links: {cnt}")

        if link.rel == pystac.RelType.CHILD:
            sub_cat = pystac.read_file(link.target)

            for _link in sub_cat.links:
                if _link.rel == pystac.RelType.CHILD:
                    catalog_links.append(_link)


    print(f"Number of catalog links: {len(catalog_links)}")
    cnt = 0

    for link in catalog_links:
        cnt += 1
        if cnt % 100 == 0:
            print(f"working catalog links: {cnt}")

        subcat = pystac.read_file(link)

        for _link in subcat.links:
            if _link.rel == pystac.RelType.CHILD:
                item = pystac.read_file(_link.target)
                item_list.append(item)

    print(f"Number of features: {len(item_list)}")

    # Geopandas ignores list-valued keys when opening, so this moves asset hrefs to properties for convenience
    for item in item_list:
        item.clear_links()
        asset_hrefs = pd.DataFrame(
            item.to_dict()['assets']).T['href'].to_dict()
        item.properties.update(asset_hrefs)

    items = pystac.ItemCollection(item_list)
    items.save_object('/data/ArcticDem/strips.geojson')
