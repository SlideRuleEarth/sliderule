from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import dbutils
import pyarrow.compute
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
    app.teardown_appcontext(close_atl13)

####################
# APIs
####################

#
# ATL13
#
@atl13.route('/ATL13', methods=['GET', 'POST'])
def atl13_route():
    single_lake = True
    try:
        # get parameters
        data = request.get_json()
        refid = data.get('refid') # reference id
        name = data.get('name') # lake name
        coord = data.get('coord') # coordinate contained in lake
        name_filter = dbutils.get_icesat2_name_filter(data)
        # get metadata
        db, mappings = __get_atl13()
        # perform database query
        if refid != None:
            table = db.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ATL13refID == {refid}
                {dbutils.build_name_filter("AND", name_filter)};
            """).fetch_arrow_table().slice(0, 1)
        elif name != None:
            table = db.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE Lake_name == '{name}'
                {dbutils.build_name_filter("AND", name_filter)};
            """).fetch_arrow_table().slice(0, 1)
        elif coord != None:
            table = db.execute(f"""
                SELECT *
                FROM atl13_mask
                WHERE ST_Contains(geometry, ST_Point({coord['lon']}, {coord['lat']}))
                {dbutils.build_name_filter("AND", name_filter)};
            """).fetch_arrow_table().slice(0, 1)
        elif name_filter != None:
            single_lake = False
            state = {'WHERE': False}
            table = db.execute(f"""
                SELECT *
                FROM atl13_mask
                {dbutils.build_name_filter(state, name_filter)};
            """).fetch_arrow_table().slice(0, 1)
        else:
            raise RuntimeError("must supply at least one query parameter (refid, name, coord, name_filter)")
        # build single lake response
        if single_lake:
            response = {
                "refid": int(table.column("ATL13refID")[0]),
                "hylak": int(table.column("Hylak_id")[0]),
                "name": int(table.column("Lake_name")[0]),
                "country": int(table.column("Country")[0]),
                "continent": int(table.column("Continent")[0]),
                "type": int(table.column("Lake_type")[0]),
                "area": table.column("Lake_area")[0],
                "basin": int(table.column("Basin_ID")[0]),
                "source": table.column("Source")[0],
                "granules": []
            }
            for granule_id in mappings["refids"][str(response["refid"])]:
                response["granules"].append(mappings["granules"][str(granule_id)])
            return json.dumps(response)
        # build multiple lake response
        else:
            hits = len(table)
            if hits > current_app.config['MAX_RESOURCES']:
                raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
            else:
                granules_column = table.column("granule")
                unique_granules = pyarrow.compute.unique(granules_column)
                response = {"hits": hits, "granules":unique_granules.to_pylist()}
            return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL13: {e}')
