from pystac_client import Client
import json
import sliderule

#
# Build a GeoJSON Response from STAC Query Response
#
def __build_geojson(rsps):
    rsps = json.dumps(rsps)
    geojson = json.loads(rsps)

    for i in reversed(range(len(geojson["features"]))):
        del geojson["features"][i]["links"]
        del geojson["features"][i]["stac_version"]
        # del geojson["features"][i]["stac_extensions"]
        del geojson["features"][i]["collection"]
        del geojson["features"][i]["bbox"]
        del geojson["features"][i]["properties"]["title"]
        del geojson["features"][i]["properties"]["created"]
        del geojson["features"][i]["properties"]["updated"]
        propertiesDict = geojson["features"][i]["properties"]
        assetsDict = geojson["features"][i]["assets"]
        for val in assetsDict:
            if "href" in assetsDict[val]:
                propertiesDict['url'] = assetsDict[val]["href"]

        del geojson["features"][i]["assets"]
    return geojson



stac_endopoint = 'https://services.terrascope.be/stac/'

# collection ids for both maps in the Terrascope STAC Catalogue
collections = ["urn:eop:VITO:ESA_WorldCover_10m_2020_AWS_V1", "urn:eop:VITO:ESA_WorldCover_10m_2021_AWS_V2"]

# Pick up results from both 2020 and 2021.
time_start = "2020-01-01T00:00:00Z"
time_end   = "2021-12-31T23:59:59Z"
timeRange = '{}/{}'.format(time_start, time_end)

coordinates = [[[coord["lon"], coord["lat"]] for coord in polygon] for polygon in [sliderule.toregion("tests/data/grandmesa.geojson")["poly"]]]
geometry = {"type": "Polygon", "coordinates": coordinates}

client = Client.open(stac_endopoint)
search_results = client.search(collections=collections, datetime=timeRange, intersects=geometry)
rsp = search_results.get_all_items_as_dict()

#for debugging, write stac endpoint response to file
all_items = json.loads(json.dumps(rsp))
jsonFile = 'worldcover.json'
with open(jsonFile, 'w') as outfile:
    json.dump(all_items, outfile)

geojson = __build_geojson(rsp)

#for debugging, write trimmed stac endpoint response to file
jsonFile = 'grand_mesa_10m_worldcover.json'
with open(jsonFile, 'w') as outfile:
    json.dump(geojson, outfile)
