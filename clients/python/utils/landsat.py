#
# HLS LANDSAT test
from pystac_client import Client
import json
import os
import requests
import boto3
import rasterio
import rioxarray
import os
from rasterio.session import AWSSession

def BuildSquare(lon, lat, delta):
    c1 = [lon + delta, lat + delta]
    c2 = [lon + delta, lat - delta]
    c3 = [lon - delta, lat - delta]
    c4 = [lon - delta, lat + delta]

    # This order matters for query to use 'inside of polygon' area
    geometry = {"type": "Polygon", "coordinates": [[ c1, c4, c3, c2, c1 ]]}
    return geometry


s3_cred_endpoint = {
    'lpdaac':'https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials'
}

def get_temp_creds(provider):
    return requests.get(s3_cred_endpoint[provider]).json()



###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    catalog = Client.open("https://cmr.earthdata.nasa.gov/stac/LPCLOUD")

    timeRange = '2021-01-01/2021-02-01'
    geometry  = BuildSquare(-178, 50, 1)
    mybbox    = [-179,41,-177,51]

    # print("Searching with bbox...")
    # results = catalog.search(collections=['HLSS30.v2.0', 'HLSL30.v2.0'],
    #                          bbox = mybbox,
    #                          datetime=timeRange)
    # print(f"Results matched: {results.matched()}")


    print("Searching with Intersects...")
    results = catalog.search(collections=['HLSS30.v2.0', 'HLSL30.v2.0'],
                             datetime=timeRange,
                             intersects=geometry,
                             fields=['HORIZONTAL_CS_CODE', 'HORIZONTAL_CS_NAME'])

    # print(f"{results.url_with_parameters()}")
    print(f"Results matched: {results.matched()}")

    itemsDict = results.get_all_items_as_dict()

    # Dumped original stack item collection to file, for testing
    file = 'hls.geojson'
    print(f"Writing reults to file {file}")
    with open(file, 'w') as fp:
        json.dump(itemsDict, fp)

    urlList = []


    for i in reversed(range(len(itemsDict["features"]))):
        del itemsDict["features"][i]["links"]
        del itemsDict["features"][i]["stac_version"]
        del itemsDict["features"][i]["stac_extensions"]
        del itemsDict["features"][i]["collection"]
        del itemsDict["features"][i]["bbox"]
        del itemsDict["features"][i]["assets"]["browse"]

        propertiesDict = itemsDict["features"][i]["properties"]
        assetsDict     = itemsDict["features"][i]["assets"]
        metaFileUrl    = assetsDict["metadata"]["href"]
        # we may have to read metaFile, get some algo related attributes from it
        # and add them to properties ie:
        # propertiesDict['algo1'] = someAlgo1value

        del itemsDict["features"][i]["assets"]["metadata"]

        for val in assetsDict:
            if "href" in assetsDict[val]:
                # add raster url to properties
                propertiesDict[val] = assetsDict[val]["href"]
                # Only needed for testing temp credentials
                urlList.append(assetsDict[val]["href"])

        del itemsDict["features"][i]["assets"]



    # Dump trimmed dictionary as geojson file, for testing
    file = 'hls_trimmed.geojson'
    print(f"Writing reults to file {file}")
    with open(file, 'w') as fp:
        json.dump(itemsDict, fp)


    ########################################################################
    ########################################################################
    ########################################################################
    # Code below tests opening rasters with AWS temp credentials from LPDAAC
    ########################################################################
    ########################################################################
    ########################################################################
    print("Getting credentials with netrc")
    if os.path.isfile(os.path.expanduser('~/.netrc')):
        temp_creds_req = get_temp_creds('lpdaac')

    print("Getting AWS session tokens...")
    session = boto3.Session(aws_access_key_id=temp_creds_req['accessKeyId'],
                            aws_secret_access_key=temp_creds_req['secretAccessKey'],
                            aws_session_token=temp_creds_req['sessionToken'],
                            region_name='us-west-2')


    # NOTE: Using rioxarray assumes you are accessing a GeoTIFF
    rio_env = rasterio.Env(AWSSession(session),
                    GDAL_DISABLE_READDIR_ON_OPEN='TRUE',
                    GDAL_HTTP_COOKIEFILE=os.path.expanduser('~/cookies.txt'),
                    GDAL_HTTP_COOKIEJAR=os.path.expanduser('~/cookies.txt'))

    rio_env.__enter__()

    s3List = []
    for e in urlList:
        #print(e)
        s3path = e.replace("https://data.lpdaac.earthdatacloud.nasa.gov/", "s3://")
        #print(s3path)
        s3List.append(s3path)

    # Open one raster and print some info, validates if this is possible with temp credentials
    for e in s3List:
        print(e)
        if '.tif' in e:
            da = rioxarray.open_rasterio(e)
            print(da)
            break


    print("Done!")
