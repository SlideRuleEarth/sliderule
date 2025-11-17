#
# Asset Metadata Service
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
import json
import duckdb

####################
# Initialization
####################

atl24 = Blueprint('atl24', __name__, url_prefix='/ams')

####################
# Module Functions
####################

def __get_atl24():
    if 'atl24db' not in g:
        g.atl24db = duckdb.connect(current_app.config['ATL24_DB'], read_only=True)
        g.atl24db.execute("LOAD spatial;")
    return g.atl24db

def close_atl24(e=None):
    db = g.pop('atl24db', None)
    if db is not None:
        db.close()

def init_app(app):
    db = duckdb.connect()
    db.execute("INSTALL spatial;")
    db.close()
    app.teardown_appcontext(close_atl24)

####################
# Helper Functions
####################

#
# Check State
#
def check_state(state):
    if not state['WHERE']:
        state['WHERE'] = True
        return 'WHERE'
    else:
        return 'AND'

#
# Build Time Query
#
def build_time_query(state):
    t0 = request.args.get('t0')
    t1 = request.args.get('t1')
    if t0 != None and t1 != None:
        return f"{check_state(state)} begin_time BETWEEN '{t0}' AND '{t1}'"
    elif t0 != None:
        return f"{check_state(state)} begin_time >= '{t0}'"
    elif t1 != None:
        return f"{check_state(state)} begin_time <= '{t1}'"
    else:
        return ''

#
# Build Range Query
#
def build_range_query(state, db_field, r0_field, r1_field):
    r0 = request.args.get(r0_field)
    r1 = request.args.get(r1_field)
    if r0 != None and r1 != None:
        return f"{check_state(state)} {db_field} BETWEEN {r0} AND {r1}"
    elif r0 != None:
        return f'{check_state(state)} {db_field} >= {r0}'
    elif r1 != None:
        return f'{check_state(state)} {db_field} <= {r1}'
    else:
        return ''

#
# Build Matching Query
#
def build_matching_query(state, db_field, m_field):
    m = request.args.get(m_field)
    if m != None:
        return f'{check_state(state)} {db_field} == {m}'
    else:
        return ''

#
# Build Geometry Query
#
def build_geometry_query(state, poly_field):
    poly = request.args.get(poly_field)
    if poly != None:
        points = poly.split(' ')
        coords = [f'{points[i]} {points[i+1]}' for i in range(0, len(points), 2)]
        poly_str = f"POLYGON(({','.join(coords)}))"
        return f"{check_state(state)} ST_Intersects(geometry, ST_GeomFromText('{poly_str}'));"
    else:
        return ''

####################
# APIs
####################

#
# ATL24
#
@atl24.route('/atl24', methods=['GET'])
def atl24_route():
    try:
        # execute query
        state = {'WHERE': False}
        db = __get_atl24()
        cmd = f"""
            SELECT *
            FROM atl24db
            {build_time_query(state)}
            {build_matching_query(state, "season", "season")}
            {build_range_query(state, "bathy_photons", "photons0", "photons1")}
            {build_range_query(state, "bathy_mean_depth", "meandepth0", "meandepth1")}
            {build_range_query(state, "bathy_min_depth", "mindepth0", "mindepth1")}
            {build_range_query(state, "bathy_max_depth", "maxdepth0", "maxdepth1")}
            {build_geometry_query(state, "poly")}
        """
        print(cmd)
        df = db.execute(cmd).df()
        # build response
        hits = len(df)
        response = {"hits": hits}
        if hits > current_app.config['MAX_RESOURCES']:
            raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
        elif hits <= 0:
            raise RuntimeError(f"no resources were found")
        else:
            response["granules"] = {}
            granules = df["granule"].unique()
            for granule in granules:
                response["granules"][granule] = list(df[df["granule"] == granule]["beam"].unique())
        # return response
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL24 metadata service: {e}')

#
# Granule
#
@atl24.route('/atl24/granule/<name>', methods=['GET'])
def granule_route(name):
    try:
        # execute query
        db = __get_atl24()
        df = db.execute(f"""
            SELECT *
            FROM atl24db
            WHERE granule == '{name}'
        """).df()
        # build response
        response = {}
        for i in range(len(df)):
            row = df.iloc[i]
            response[row["beam"]] = {
                "season": int(row["season"]),
                "bathy_photons": int(row["bathy_photons"]),
                "bathy_mean_depth": float(row["bathy_mean_depth"]),
                "bathy_min_depth": float(row["bathy_min_depth"]),
                "bathy_max_depth": float(row["bathy_max_depth"]),
                "begin_time": row["begin_time"].isoformat()
            }
        # return response
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL24 metadata service: {e}')
