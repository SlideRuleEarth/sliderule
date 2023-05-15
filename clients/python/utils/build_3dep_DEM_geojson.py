import requests
import json


"""
   chrome-extension://efaidnbmnnnibpcajpcglclefindmkaj/https://apps.nationalmap.gov/help/documents/TNMAccessAPIDocumentation/TNMAccessAPIDocumentation.pdf

   Based on User Selection Area of Interest (AOI), feed coordinates to USGS
   National Map API (https://tnmaccess.nationalmap.gov/api/v1/products')
   This will return a JSON of all geotiffs that intersect the AOI
"""


#INPUTS
#----------------------------------------------------------------------
minlon =  -108.30871582031250
maxlon =  -107.87406921386717

minlat =    38.81670618152057
maxlat =    39.12260286627899


dataset = 'Digital Elevation Model (DEM) 1 meter'


#put bounding box into string format
bbox = (minlon,minlat,maxlon,maxlat)
bbox = str(bbox)
bbox = bbox.strip('()')

#Build call to USGS REST API
url='https://tnmaccess.nationalmap.gov/api/v1/products'

rxItemsCnt = 0
requestCnt = 0
allItems = []

#Execute REST API calls to collect all 3DEP 1 meter DEM rasters for AOI.
while requestCnt >= 0:
    params = dict(datasets=dataset, bbox=bbox, prodFormats='GeoTIFF',outputFormat='JSON', max=100, offset=rxItemsCnt)

    try:
        r = requests.get(url,params=params)
    except:
        print('Error with Service Call')

    #load API JSON output into a variable
    try:
        data = json.loads(r.content)
    except:
        print('Error loading JSON output')

    #for debugging, write json server response to file
    #jsonFile = 'dem' + str(requestCnt) + '.json'
    #with open(jsonFile, 'w') as outfile:
    #    json.dump(data, outfile)

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

    #clockwise for interior of bounding box
    coordList = [[minX, minY], [minX, maxY], [maxX, maxY], [maxX, minY], [minX, minY]]
    geometryDic = {"type": "Polygon", "coordinates": coordList}
    geojsonDict['features'][i].update({"geometry": geometryDic})

    date = allItems[i]['lastUpdated']
    url  = allItems[i]['urls']['TIFF'].replace('https://prd-tnm.s3.amazonaws.com','/vsis3/prd-tnm')
    propertiesDict = {"datetime": date, "url": url }
    geojsonDict['features'][i].update({"properties": propertiesDict})


#for debugging, write geojson to file
geojsonFile = '3dep.geojson'
with open(geojsonFile, 'w') as outfile:
    json.dump(geojsonDict, outfile)
