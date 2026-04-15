#
# See sliderule/clients/python/utils/atl13_utils.py for code that creates the metadata database
# See sliderule/clients/python/utils/atl13_generate_mappings for code that creates the granule mappings
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import dbutils
import json
import duckdb

####################
# Initialization
####################

atl13 = Blueprint('atl13', __name__, url_prefix='/ams')

print("Initializing ATL13 metadata services...")

####################
# Module Functions
####################

def _get_atl13():
    if 'db' not in g:
        g.db = duckdb.connect(current_app.config['ATL13_DB'], read_only=True)
        g.db.execute("LOAD spatial;")
    return g.db

def close_atl13(e=None):
    db = g.pop('db', None)
    if db is not None:
        db.close()

def init_app(app):
    app.teardown_appcontext(close_atl13)

####################
# Local  Functions
####################

#
# Query by Reference ID
#
def _query_by_refid(db, data):
    refid = data.get('refid') # reference id
    granules = refid and db.execute(f"""
        WITH ids AS (
            SELECT UNNEST(granules) AS gid
            FROM atl13refids
            WHERE ATL13refID = $1
        )
        SELECT g.granules
        FROM ids
        JOIN atl13gids g ON g.ids = ids.gid
    """, [refid]).fetchnumpy()["granules"]
    return refid, granules

#
# Query by Lake Name
#
def _query_by_name(db, data):
    name = data.get('name') # lake name
    table = name and db.execute(f"""
        WITH ids AS (
            SELECT UNNEST(granules) AS gid, ATL13refID
            FROM atl13refids
            WHERE Lake_name == $1
        )
        SELECT ids.ATL13refID, g.granules
        FROM ids
        JOIN atl13gids g ON g.ids = ids.gid
    """, [name]).fetchnumpy()
    if table and len(table["ATL13refID"]) > 0 and (table["ATL13refID"] == table["ATL13refID"][0]).all():
        return table["ATL13refID"][0], table["granules"]
    else:
        return None, None

#
# Query by Containing Coordinate
#
def _query_by_coord(db, data):
    coord = data.get('coord') # coordinate contained in lake
    table = coord and db.execute(f"""
        WITH ids AS (
            SELECT UNNEST(granules) AS gid, ATL13refID
            FROM atl13refids
            WHERE ST_Contains(geometry, ST_Point($1, $2))
        )
        SELECT ids.ATL13refID, g.granules
        FROM ids
        JOIN atl13gids g ON g.ids = ids.gid
    """, [coord['lon'], coord['lat']]).fetchnumpy()
    if table and len(table["ATL13refID"]) > 0 and (table["ATL13refID"] == table["ATL13refID"][0]).all():
        return table["ATL13refID"][0], table["granules"]
    else:
        return None, None

#
# Query Multiple Lakes
#
def _query_multiple(db, data):
    state = {'WHERE': False}
    name_filter = dbutils.get_icesat2_name_filter(data)
    if name_filter or ("poly" in data) or ("begin_time" in data):
        return db.execute(f"""
            SELECT granule
            FROM atl13granules
            {dbutils.build_name_filter(state, name_filter)}
            {dbutils.build_polygon_query(state, data)}
            {dbutils.build_time_query(state, data)}
        """).fetchnumpy()["granule"]
    else:
        return None

####################
# APIs
####################

#
# ATL13
#
@atl13.route('/ATL13', methods=['GET', 'POST'])
def atl13_route():
    try:
        data = request.get_json()
        db = _get_atl13()
        # perform single lake query
        for query in [_query_by_refid, _query_by_name, _query_by_coord]:
            lake_refid, lake_granules = query(db, data)
            if lake_refid: break # lake has been found
        # perform general query
        global_granules = _query_multiple(db, data)
        if global_granules is not None:
            granules = list(set(global_granules) & set(lake_granules))
        else:
            granules = lake_granules.tolist()
        # return response
        return json.dumps({
            "hits": len(granules),
            "refid": int(lake_refid),
            "granules": granules
        })
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ATL13: {e}')
