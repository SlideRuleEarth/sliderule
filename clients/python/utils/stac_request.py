from pystac_client import Client
import sliderule
import requests
import json

# parameters
debug = False
client = False
manual = True
page = None
url = 'https://cmr.earthdata.nasa.gov/stac/LPCLOUD'
headers = {'Content-Type': 'application/json'}
limit = 200
collections = ["HLSS30.v2.0", "HLSL30.v2.0"]
time_start = "2021-01-01T00:00:00Z"
time_end = "2022-01-01T23:59:59Z"
coordinates = [[[coord["lon"], coord["lat"]] for coord in polygon] for polygon in [sliderule.toregion("examples/grandmesa.geojson")["poly"]]]

# debug output
if debug:
    import logging
    import http.client as http_client
    http_client.HTTPConnection.debuglevel = 1
    logging.basicConfig()
    logging.getLogger().setLevel(logging.DEBUG)
    requests_log = logging.getLogger("requests.packages.urllib3")
    requests_log.setLevel(logging.DEBUG)
    requests_log.propagate = True


# client stac request
if client:
    catalog = Client.open(url)
    timeRange = '{}/{}'.format(time_start, time_end)
    geometry = {"type": "Polygon", "coordinates": coordinates}
    results = catalog.search(collections=collections, datetime=timeRange, intersects=geometry)
    items = results.get_all_items_as_dict()
    print(f"Returned {results.matched()} features")


# manual stac request
if manual:
    rqst = {
        "limit": 100,
        "datetime": '{}/{}'.format(time_start, time_end),
        "collections": collections,
        "intersects": {
            "type": "Polygon",
            "coordinates": coordinates
        }
    }
    if page:
        rqst["page"] = page
    context = requests.Session()
    context.trust_env = False
    data = context.post(url+"/search", data=json.dumps(rqst), headers=headers)
    data.raise_for_status()
    geojson = data.json()
    print("Returned {} features".format(geojson["context"]["returned"]))

