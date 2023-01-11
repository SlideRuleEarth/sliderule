#
# Test for landsat stac server
import geopandas as gpd
import requests as r
import boto3
import rasterio as rio
import rioxarray
import os
from rasterio.session import AWSSession

def BuildSquare(lon, lat, delta):
    c1 = [lon + delta, lat + delta]
    c2 = [lon + delta, lat - delta]
    c3 = [lon - delta, lat - delta]
    c4 = [lon - delta, lat + delta]
    geometry = {"type": "Polygon", "coordinates": [[ c1, c2, c3, c4, c1 ]]}
    return geometry


s3_cred_endpoint = {
    'lpdaac':'https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials'
}

def get_temp_creds(provider):
    return r.get(s3_cred_endpoint[provider]).json()



###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    stac = 'https://cmr.earthdata.nasa.gov/stac/' # CMR-STAC API Endpoint
    stac_response = r.get(stac).json()            # Call the STAC API endpoint

    # for s in stac_response: print(s)

    print(f"You are now using the {stac_response['id']} API (STAC Version: {stac_response['stac_version']}). \n{stac_response['description']}")
    print(f"There are {len(stac_response['links'])} STAC catalogs available in CMR.")

    stac_lp = [s for s in stac_response['links'] if 'LP' in s['title']]  # Search for only LP-specific catalogs

    # LPCLOUD is the STAC catalog we will be using and exploring today
    lp_cloud = r.get([s for s in stac_lp if s['title'] == 'LPCLOUD'][0]['href']).json()
    # for l in lp_cloud: print(f"{l}: {lp_cloud[l]}")

    lp_links = lp_cloud['links']
    for l in lp_links:
        try:
            print(f"{l['href']} is the {l['title']}")
        except:
            print(f"{l['href']}")


    lp_collections = [l['href'] for l in lp_links if l['rel'] == 'collections'][0]  # Set collections endpoint to variable
    collections_response = r.get(f"{lp_collections}").json()                        # Call collections endpoint
    print(f"This collection contains {collections_response['description']} ({len(collections_response['collections'])} available)")

    collections = collections_response['collections']
    # print(collections[1])

    # Search available version 2 collections for HLS and print them out
    hls_collections = [c for c in collections if 'HLS' in c['id'] and 'v2' in c['id']]
    for h in hls_collections:
        print(f"{h['title']} has an ID (shortname) of: {h['id']}")

    l30 = [h for h in hls_collections if 'HLSL30' in h['id'] and 'v2.0' in h['id']][0]     # Grab HLSL30 collection
    for l in l30['extent']:                                                                # Check out the extent of this collection
        print(f"{l}: {l30['extent'][l]}")

    l30_id = 'HLSL30.v2.0'
    print(f"HLS L30 Start Date is: {l30['extent']['temporal']['interval'][0][0]}")

    l30_items = [l['href'] for l in l30['links'] if l['rel'] == 'items'][0]    # Set items endpoint to variable
    print(l30_items)
    l30_items_response = r.get(f"{l30_items}").json()    # Call items endpoint
    # print(l30_items_response)
    # l30_item = l30_items_response['features'][0]         # select first item (10 items returned by default)
    # print(l30_item)

    # for i, l in enumerate(l30_items_response['features']):
    #     print(f"Item at index {i} is {l['id']}")
        # print(f"Item at index {i} is {l['properties']['eo:cloud_cover']}% cloudy.")

    lp_search = [l['href'] for l in lp_links if l['rel'] == 'search'][0]    # Define the search endpoint
    # lp_search is  https://cmr.earthdata.nasa.gov/stac/LPCLOUD/search

    # Set up a dictionary that will be used to POST requests to the search endpoint
    params = {}

    lim = 100
    params['limit'] = lim  # Add in a limit parameter to retrieve 100 items at a time.
    print(params)
    search_response = r.post(lp_search, json=params).json()  # send POST request to retrieve first 100 items in the STAC collection
    print(f"{len(search_response['features'])} items found!")

    # Bring in the farm field region of interest
    field = gpd.read_file('/home/elidwa/hls-tutorial/Field_Boundary.geojson')
    print(field)
    fieldShape = field['geometry'][0] # Define the geometry as a shapely polygon
    bbox = f'{fieldShape.bounds[0]},{fieldShape.bounds[1]},{fieldShape.bounds[2]},{fieldShape.bounds[3]}'    # Defined from ROI bounds
    params['bbox'] = bbox                                                                                    # Add ROI to params
    date_time = "2021-07-01T00:00:00Z/2021-08-31T23:59:59Z"    # Define start time period / end time period
    params['datetime'] = date_time
    params['collections'] = l30_id

    hls_items = r.post(lp_search, json=params).json()   # Send POST request with datetime included
    print(f"{len(hls_items['features'])} items found!")
    hls_items = hls_items['features']

    h = hls_items[0]
    # print(h)

    evi_band_links = []

    # Define which HLS product is being accessed
    # if h['assets']['browse']['href'].split('/')[4] == 'HLSS30.015':
    #     evi_bands = ['B8A', 'B04', 'B02', 'Fmask'] # NIR RED BLUE Quality for S30
    # else:
    #     evi_bands = ['B05', 'B04', 'B02', 'Fmask'] # NIR RED BLUE Quality for L30

    evi_bands = ['B05', 'B04', 'B02', 'Fmask'] # NIR RED BLUE Quality for L30

    # Subset the assets in the item down to only the desired bands
    for a in h['assets']:
        if any(b == a for b in evi_bands):
            evi_band_links.append(h['assets'][a]['href'])
    for e in evi_band_links: print(e)

# The result of this seach is:
# https://data.lpdaac.earthdatacloud.nasa.gov/lp-prod-protected/HLSL30.020/HLS.L30.T10TEK.2021183T185121.v2.0/HLS.L30.T10TEK.2021183T185121.v2.0.B02.tif
# https://data.lpdaac.earthdatacloud.nasa.gov/lp-prod-protected/HLSL30.020/HLS.L30.T10TEK.2021183T185121.v2.0/HLS.L30.T10TEK.2021183T185121.v2.0.Fmask.tif
# https://data.lpdaac.earthdatacloud.nasa.gov/lp-prod-protected/HLSL30.020/HLS.L30.T10TEK.2021183T185121.v2.0/HLS.L30.T10TEK.2021183T185121.v2.0.B04.tif
# https://data.lpdaac.earthdatacloud.nasa.gov/lp-prod-protected/HLSL30.020/HLS.L30.T10TEK.2021183T185121.v2.0/HLS.L30.T10TEK.2021183T185121.v2.0.B05.tif
#
# create an s3 list
    s3List = []

    for e in evi_band_links:
        # print(e)
        s3path = e.replace("https://data.lpdaac.earthdatacloud.nasa.gov/", "s3://")
        # print(s3path)
        s3List.append(s3path)

    for e in s3List:
        print(e)


    if os.path.isfile(os.path.expanduser('~/.netrc')):
        # For Githhub CI, we can use ~/.netrc
        temp_creds_req = get_temp_creds('lpdaac')
    else:
        # ADD temporary credentials here
        temp_creds_req = {}

    session = boto3.Session(aws_access_key_id=temp_creds_req['accessKeyId'],
                            aws_secret_access_key=temp_creds_req['secretAccessKey'],
                            aws_session_token=temp_creds_req['sessionToken'],
                            region_name='us-west-2')


    # NOTE: Using rioxarray assumes you are accessing a GeoTIFF
    rio_env = rio.Env(AWSSession(session),
                    GDAL_DISABLE_READDIR_ON_OPEN='TRUE',
                    GDAL_HTTP_COOKIEFILE=os.path.expanduser('~/cookies.txt'),
                    GDAL_HTTP_COOKIEJAR=os.path.expanduser('~/cookies.txt'))
    rio_env.__enter__()


    for e in s3List:
        print(e)
        if '.tif' in e:
            da = rioxarray.open_rasterio(e)
            print(da)


    print("Done!")
