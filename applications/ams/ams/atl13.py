from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import icesat2
import threading
import time
import json
import duckdb

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

####################
# APIs
####################

#
# ATL13
#
@atl13.route('/atl13', methods=['GET', 'POST'])
def atl13_route():
    single_lake = True
    try:
        # get parameters
        data = request.get_json()
        refid = data.get('refid') # reference id
        name = data.get('name') # lake name
        coord = data.get('coord') # coordinate contained in lake
        poly = None # icesat2.get_polygon_query(data) # currently unsupported by AMS
        name_filter = icesat2.get_name_filter(data)
        # get metadata
        mask, mappings = __get_atl13()
        print("DATA", data)
        # perform database query
        if refid != None:
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ATL13refID == {refid}
                {icesat2.build_polygon_query("AND", poly)};
            """).df().iloc[0]
        elif name != None:
            print("CMD", f"""
                SELECT *
                FROM atl13_mask
                WHERE Lake_name == '{name}'
                {icesat2.build_polygon_query("AND", poly)};
            """)
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE Lake_name == '{name}'
                {icesat2.build_polygon_query("AND", poly)};
            """).df().iloc[0]
        elif coord != None:
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ST_Contains(geometry, ST_Point({coord['lon']}, {coord['lat']}))
                {icesat2.build_polygon_query("AND", poly)};
            """).df().iloc[0]
        elif name_filter != None and poly != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {icesat2.build_name_filter("WHERE", name_filter)}
                {icesat2.build_polygon_query("AND", poly)};
            """).df()
        elif name_filter != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {icesat2.build_name_filter("AND", name_filter)};
            """).df()
        elif poly != None:
            single_lake = False
            data = mask.execute(f"""
                SELECT *
                FROM atl13_mask
                {icesat2.build_polygon_query("WHERE", poly)};
            """).df()
        else:
            raise RuntimeError("must supply at least one query parameter (refid, name, coord, poly, name_filter)")
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
            if hits > current_app.config['MAX_RESOURCES']:
                raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
            else:
                response = {"hits": hits, "granules":list(data["granule"].unique())}
            return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL13: {e}')
