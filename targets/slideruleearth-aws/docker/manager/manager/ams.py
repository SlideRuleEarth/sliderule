#
# Asset Metadata Service
#

from flask import (Blueprint, request, current_app)
from werkzeug.exceptions import abort
import time
import json
import geopandas as gpd
from shapely.geometry import Point

####################
# Initialization
####################

atl13_mask = None
atl13_mappings = None

ams = Blueprint('ams', __name__, url_prefix='/manager/ams')

####################
# Module Functions
####################

def load_database():
    global atl13_mask, atl13_mappings
    # read mappings
    with open(current_app.config['ATL13_MAPPINGS'], "r") as file:
        print(f'Loading ATL13 mappings... ', end='')
        start_time = time.perf_counter()
        atl13_mappings = json.loads(file.read())
        print(f'completed in {time.perf_counter() - start_time:.2f} secs.')
    # read body mask
    print(f'Loading ATL13 body mask... ', end='')
    start_time = time.perf_counter()
    atl13_mask = gpd.read_parquet(current_app.config['ATL13_MASK'])
    print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

def get_ams():
    global atl13_mask, atl13_mappings
    if type(atl13_mask) == type(None):
        load_database()
    return {
        "atl13_mask": atl13_mask,
        "atl13_mappings": atl13_mappings
    }

def close_ams(e=None):
    pass

def init_app(app):
    app.teardown_appcontext(close_ams)

####################
# APIs
####################

#
# ATL13
#
@ams.route('/atl13', methods=['GET'])
def atl13():
    try:
        # get parameters
        refid = request.args.get('refid') # reference id
        name = request.args.get('name') # lake name
        lon = request.args.get('lon') # longitude coordinate
        lat = request.args.get('lat') # latitude coordinate
        # get metadata
        metadata = get_ams()
        mask = metadata["atl13_mask"]
        mappings = metadata["atl13_mappings"]
        # get refid
        if refid != None:
            refid = int(refid) # directly supplied
        elif name != None:
            refid = mask[mask["Lake_name"] == name].index[0]
        elif lon != None and lat != None:
            refid = mask[mask.geometry.contains(Point(lon, lat))].index[0]
        else:
            raise RuntimeError("must supply at least one query parameter (refid, name, lon and lat)")
        # build response
        response = {
            "refid": int(refid),
            "hylak": int(mask.loc[refid, "Hylak_id"]),
            "name": mask.loc[refid, "Lake_name"],
            "country": mask.loc[refid, "Country"],
            "continent": mask.loc[refid, "Continent"],
            "type": int(mask.loc[refid, "Lake_type"]),
            "area": mask.loc[refid, "Lake_area"],
            "basin": int(mask.loc[refid, "Basin_ID"]),
            "source": mask.loc[refid, "Source"],
            "granules": []
        }
        for granule_id in mappings["refids"][str(refid)]:
            response["granules"].append(mappings["granules"][str(granule_id)])
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL13 metadata service: {e}')
