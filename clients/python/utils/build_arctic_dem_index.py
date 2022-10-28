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

    collection_stac = 'https://pgc-opendata-dems.s3.us-west-2.amazonaws.com/arcticdem/mosaics/v3.0/2m.json'
    col = pystac.read_file(collection_stac)

    catalog_links = []
    item_list = []

    for link in col.links:
        if link.rel == pystac.RelType.CHILD:
            catalog_links.append(link.target)

    print(f"Number of tiles: {len(catalog_links)}")

    cnt = 0

    for link in catalog_links:
        subcat = pystac.read_file(link)
        print(subcat)
        # cnt += 1
        # if cnt == 20:
        #     break

    # the call on catalog get_all_items() should find all items/features from this and all child catalog and collections
    # for some reason the recursion breaks with an error that item does not have property get_all_items()
    # this may be caused by bug in pystac or related to how data is presented for arcticdem, don't know
    # item_list = list(subcat.get_all_items())

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
    items.save_object('/data/ArcticDem/mosaic.geojson')
