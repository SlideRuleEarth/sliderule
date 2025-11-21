#
# Asset Metadata Service
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
import time
import json
import duckdb
import threading

####################
# Initialization
####################

metadata = {}
metalock = threading.Lock()

atl13 = Blueprint('atl13', __name__, url_prefix='/ams')

####################
# Module Functions
####################

def __get_atl13():
    global metadata, metalock
    # get mask
    if 'db' not in g:
        g.db = duckdb.connect(current_app.config['ATL13_MASK'], read_only=True)
        g.db.execute("LOAD spatial;")
    # get mappings
    with metalock:
        if "atl13" not in metadata:
            with open(current_app.config['ATL13_MAPPINGS'], "r") as file:
                print(f'Loading ATL13 mappings... ', end='')
                start_time = time.perf_counter()
                metadata["atl13"] = {}
                metadata["atl13"]["mappings"] = json.loads(file.read())
                print(f'completed in {time.perf_counter() - start_time:.2f} secs.')
    return g.db, metadata["atl13"]["mappings"]

def close_atl13(e=None):
    db = g.pop('db', None)
    if db is not None:
        db.close()

def init_app(app):
    db = duckdb.connect()
    db.execute("INSTALL spatial;")
    db.close()
    app.teardown_appcontext(close_atl13)

#
# Build Geometry Query
#
def build_geometry_query(verb, poly):
    if poly != None:
        return f"{verb} ST_Intersects(geometry, ST_GeomFromText('{poly}'));"
    else:
        return ''

#
# Build Name Filter
#
def build_name_filter(verb, name_filter):
    if name_filter != None:
        return rf"{verb} granule LIKE '{name_filter}' ESCAPE '\\'"
    else:
        return ''

####################
# APIs
####################

#
# ATL13
#
@atl13.route('/atl13', methods=['GET'])
def atl13_route():
    single_lake = True
    try:
        # get parameters
        refid = request.args.get('refid') # reference id
        name = request.args.get('name') # lake name
        lon = request.args.get('lon') # longitude coordinate
        lat = request.args.get('lat') # latitude coordinate
        poly = request.args.get("poly")
        name_filter = request.args.get("name_filter")
        # get metadata
        mask, mappings = __get_atl13()
        # reformat polygon
        if poly != None:
            points = poly.split(' ')
            coords = [f'{points[i]} {points[i+1]}' for i in range(0, len(points), 2)]
            poly = f"POLYGON(({','.join(coords)}))"
        # perform database query
        if refid != None:
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ATL13refID == {refid};
            """).df().iloc[0]
        elif name != None:
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE Lake_name == '{name}';
            """).df().iloc[0]
        elif lon != None and lat != None:
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ST_Contains(geometry, ST_Point({lon}, {lat}))
                {build_geometry_query("AND", poly)};
            """).df().iloc[0]
        elif poly != None and name_filter != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {build_geometry_query("WHERE", poly)}
                {build_name_filter("AND", name_filter)};
            """).df()
        elif poly != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {build_geometry_query("WHERE", poly)};
            """).df()
        elif name_filter != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {build_name_filter("AND", name_filter)};
            """).df()
        else:
            raise RuntimeError("must supply at least one query parameter (refid, name, lon and lat)")
        # build single lake response
        if single_lake:
            response = {
                "refid": int(data["ATL13refID"]),
                "hylak": int(data["Hylak_id"]),
                "name": data["Lake_name"],
                "country": data["Country"],
                "continent": data["Continent"],
                "type": int(data["Lake_type"]),
                "area": data["Lake_area"],
                "basin": int(data["Basin_ID"]),
                "source": data["Source"],
                "granules": []
            }
            for granule_id in mappings["refids"][str(response["refid"])]:
                response["granules"].append(mappings["granules"][str(granule_id)])
            return json.dumps(response)
        # build multiple lake response
        else:
            hits = len(data)
            response = {"hits": hits}
            if hits > current_app.config['MAX_RESOURCES']:
                raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
            elif hits <= 0:
                raise RuntimeError(f"no resources were found")
            else:
                response["granules"] = {}
                granules = data["granule"].unique()
                for granule in granules:
                    response["granules"][granule] = []
            return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL13 metadata service: {e}')
