import requests
import json
from datetime import datetime, timezone

"""
   chrome-extension://efaidnbmnnnibpcajpcglclefindmkaj/https://apps.nationalmap.gov/help/documents/TNMAccessAPIDocumentation/TNMAccessAPIDocumentation.pdf

   Based on User Selection Area of Interest (AOI), feed coordinates to USGS
   National Map API (https://tnmaccess.nationalmap.gov/api/v1/products')
   This will return a JSON of all geotiffs that intersect the AOI
"""


#INPUTS
#----------------------------------------------------------------------
dataset = 'Digital Elevation Model (DEM) 1 meter'


#the API doc states that the list should be  [x,y x,y x,y x,y x,y]  Polygon, longitude/latitude values expressed in decimal degrees in a space-delimited list.
#the doc is WRONG, the list must be          [x y, x y, x y, x y, x y]
grandMesaPoly = '-108.311682565537666 39.137576462129438,  -108.341156683252237 39.037589876133246, -108.287868638779599 38.89051431295789, -108.207729687800509 38.823205529198098, -108.074601643110313 38.847513782586297, -107.985605104949812 38.943991201101703, -107.728398587557521 39.015109302306328, -107.787241424909936 39.195630349659986, -108.049394800987542 39.139504663354245, -108.172870009708575 39.159200663961158, -108.311682565537666 39.137576462129438'

#Build call to USGS REST API
url='https://tnmaccess.nationalmap.gov/api/v1/products'

rxItemsCnt = 0
requestCnt = 0
allItems = []

#Execute REST API calls to collect all 3DEP 1 meter DEM rasters for AOI.
while requestCnt >= 0:
    params = dict(datasets=dataset, polygon=grandMesaPoly, prodFormats='GeoTIFF',outputFormat='JSON', max=100, offset=rxItemsCnt)

    try:
        r = requests.get(url,params=params)
    except:
        print('Error with Service Call')

    #load API JSON output into a variable
    try:
        data = json.loads(r.content)
    except:
        print('Error loading JSON output')

    # print(r.content)

    #for debugging, write json server response to file
    jsonFile = 'dem' + str(requestCnt) + '.json'
    with open(jsonFile, 'w') as outfile:
        json.dump(data, outfile)

    #extract just the records from the JSON
    items = data['items']
    allItems += items

    totalItems = data['total']
    thisRequestItemsCnt = len(items)
    print(f"Total Number of features: {totalItems}, received: {thisRequestItemsCnt}")

    requestCnt += 1
    rxItemsCnt += thisRequestItemsCnt

    if rxItemsCnt == totalItems:
        break


#create geojson
geojsonDict = {
    "type": "FeatureCollection",
    "features": []
    }

for i in range(len(allItems)):
    geojsonDict['features'].append({"type": "Feature"})
    id = allItems[i]['sourceId']
    geojsonDict['features'][i].update({"id": id})

    #create polygon from bounding box
    minX = allItems[i]['boundingBox']['minX']
    maxX = allItems[i]['boundingBox']['maxX']
    minY = allItems[i]['boundingBox']['minY']
    maxY = allItems[i]['boundingBox']['maxY']

    #counter-clockwise for interior of bounding box
    coordList = [[[minX, minY], [maxX, minY], [maxX, maxY], [minX, maxY], [minX, minY]]]

    geometryDic = {"type": "Polygon", "coordinates": coordList}
    geojsonDict['features'][i].update({"geometry": geometryDic})

    date = allItems[i]['lastUpdated']
    pydate = datetime.fromisoformat(date)
    date = pydate.astimezone(None).isoformat()
    url  = allItems[i]['urls']['TIFF']
    propertiesDict = {"datetime": date, "url": url }
    geojsonDict['features'][i].update({"properties": propertiesDict})


#for debugging, write geojson to file
geojsonFile = '3dep.geojson'
with open(geojsonFile, 'w') as outfile:
    json.dump(geojsonDict, outfile)
