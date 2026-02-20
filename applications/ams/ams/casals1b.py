from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import icesat2
import json
import duckdb

####################
# Initialization
####################

casals1b = Blueprint('casals1b', __name__, url_prefix='/ams')

####################
# Module Functions
####################

def __get_casals1b():
    if 'casals1bdb' not in g:
        g.casals1bdb = duckdb.connect(current_app.config['CASALS1B_DB'], read_only=True)
        g.casals1bdb.execute("LOAD spatial;")
    return g.casals1bdb

def close_casals1b(e=None):
    db = g.pop('casals1bdb', None)
    if db is not None:
        db.close()

def init_app(app):
    db = duckdb.connect()
    db.execute("INSTALL spatial;")
    db.close()
    app.teardown_appcontext(close_casals1b)

####################
# Helper Functions
####################

# Private: Check State
def _check(state):
    if type(state) == str:
        return state
    elif type(state) == dict:
        if not state['WHERE']:
            state['WHERE'] = True
            return 'WHERE'
        else:
            return 'AND'
    else:
        return ''

# Get Polygon Query
def get_polygon_query(parms):
    poly = parms.get("poly")
    if poly != None:
        coords = [f'{coord["lon"]} {coord["lat"]}' for coord in poly]
        return f"POLYGON(({','.join(coords)}))"
    else:
        return None

# Build Polygon Query
def build_polygon_query(clause, poly):
    if poly != None:
        return f"{_check(clause)} ST_Intersects(geometry, ST_GeomFromText('{poly}'))"
    else:
        return ''

# Build Time Query
def build_time_query(clause, parms):
    t0 = parms.get('t0')
    t1 = parms.get('t1')
    if t0 != None and t1 != None:
        return f"{_check(clause)} begin_time BETWEEN '{t0}' AND '{t1}'"
    elif t0 != None:
        return f"{_check(clause)} begin_time >= '{t0}'"
    elif t1 != None:
        return f"{_check(clause)} begin_time <= '{t1}'"
    else:
        return ''

####################
# APIs
####################

#
# CASALS1B
#
@casals1b.route('/CASALS1B', methods=['GET', 'POST'])
def casals1b_route():
    try:
        # execute query
        data = request.get_json()
        poly = get_polygon_query(data)
        state = {'WHERE': False}
        db = __get_casals1b()
        cmd = f"""
            SELECT *
            FROM casals1bdb
            {icesat2.build_time_query(state, data)}
            {icesat2.build_polygon_query(state, poly)}
        """
        df = db.execute(cmd).df()
        hits = len(df)
        if hits > current_app.config['MAX_RESOURCES']:
            raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
        else:
            response = {"hits": hits, "granules": list(df["granule"].unique())}
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query CASALS 1B metadata service: {e}')

#
# Search Mask
#
@casals1b.route('/CASALS1B/search_mask', methods=['GET', 'POST'])
def search_mask():
    try:
        with open(current_app.config['CASALS1B_SEARCH_MASK'], "r") as file:
            contents = file.read()
        return contents
    except Exception as e:
        abort(400, f'Failed to query CASALS 1B metadata service: {e}')
