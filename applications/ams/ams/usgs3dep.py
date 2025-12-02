from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
import json
import pandas
import duckdb

####################
# Initialization
####################

usgs3dep = Blueprint('3dep', __name__, url_prefix='/ams')

####################
# Module Functions
####################

def __get_3dep():
    if 'usgs3depdb' not in g:
        g.usgs3depdb = duckdb.connect(current_app.config['USGS3DEP_DB'], read_only=True)
        g.usgs3depdb.execute("LOAD spatial;")
    return g.usgs3depdb

def close_3dep(e=None):
    db = g.pop('usgs3depdb', None)
    if db is not None:
        db.close()

def init_app(app):
    db = duckdb.connect()
    db.execute("INSTALL spatial;")
    db.close()
    app.teardown_appcontext(close_3dep)

####################
# Helper Functions
####################

#
# Private: Check State
#
def __check(state):
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

#
# Build Polygon Query
#
def build_polygon_query(clause, parms):
    poly = parms.get("poly")
    if poly != None:
        coords = [f'{coord["lon"]} {coord["lat"]}' for coord in poly]
        poly = f"POLYGON(({','.join(coords)}))"
        return f"{__check(clause)} ST_Intersects(geometry, ST_GeomFromText('{poly}'))"
    else:
        return ''

#
# Build Time Query
#
def build_time_query(clause, parms):
    t0 = parms.get('t0')
    t1 = parms.get('t1')
    if t0 != None and t1 != None:
        return f"{__check(clause)} start_datetime BETWEEN '{t0}' AND '{t1}'"
    elif t0 != None:
        return f"{__check(clause)} start_datetime >= '{t0}'"
    elif t1 != None:
        return f"{__check(clause)} start_datetime <= '{t1}'"
    else:
        return ''

####################
# APIs
####################

#
# 3DEP
#
@usgs3dep.route('/3dep', methods=['GET', 'POST'])
def usgs3dep_route():
    try:
        # execute query
        data = request.get_json()
        state = {'WHERE': False}
        db = __get_3dep()
        df = db.execute(f"""
            SELECT *
            FROM "3depdb"
            {build_time_query(state, data)}
            {build_polygon_query(state, data)}
        """).df()
        # build response
        hits = len(df)
        response = {
            "type": "FeatureCollection",
            "features": [],
            "hits": hits
        }
        for i in range(hits):
            row = df.iloc[i]
            minX = row["bbox"]["xmin"]
            maxX = row["bbox"]["xmax"]
            minY = row["bbox"]["ymin"]
            maxY = row["bbox"]["ymax"]
            feature = {
                "type": "Feature",
                "id": row["id"],
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[minX, minY], [maxX, minY], [maxX, maxY], [minX, maxY], [minX, minY]]]
                },
                "properties": {
                    "datetime": row["start_datetime"].isoformat(),
                    "url": row["assets"]["elevation"]["href"]
                }
            }
            response["features"].append(feature)
        # return response
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query 3DEP metadata service: {e}')

#
# Id
#
@usgs3dep.route('/3dep/id/<id>', methods=['GET', 'POST'])
def id_route(id):
    try:
        # execute query
        db = __get_3dep()
        data = db.execute(f"""
            SELECT *
            FROM "3depdb"
            WHERE id == '{id}'
        """).df().iloc[0]
        # clean data
        row = {}
        for k, v in data.items():
            if hasattr(v, "tolist"):
                row[k] = v.tolist()
            elif isinstance(v, (pandas.Timestamp)):
                row[k] = v.isoformat()
            elif not isinstance(v, (bytes, bytearray)):
                row[k] = v
        # return response
        return json.dumps(row)
    except Exception as e:
        abort(400, f'Failed to query 3DEP metadata service: {e}')
