#
# See sliderule/clients/python/utils/3dep_gen_db.py for code that creates the metadata database
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import dbutils
import datetime
import json
import duckdb

####################
# Initialization
####################

usgs3dep = Blueprint('usgs3dep', __name__, url_prefix='/ams')

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
    app.teardown_appcontext(close_3dep)

####################
# APIs
####################

#
# 3DEP
#
@usgs3dep.route('/3DEP1M', methods=['GET', 'POST'])
def usgs3dep_route():
    try:
        # execute query
        data = request.get_json()
        state = {'WHERE': False}
        db = __get_3dep()
        table = db.execute(f"""
            SELECT *
            FROM "3depdb"
            {dbutils.build_time_query(state, data, "start_datetime")}
            {dbutils.build_polygon_query(state, data)}
        """).to_arrow_table()
        # build response
        hits = len(table)
        response = {
            "type": "FeatureCollection",
            "features": [],
            "hits": hits
        }
        for i in range(hits):
            row = table.slice(i, i+1)
            assets = row.column("assets")[0].as_py()
            bbox = row.column("bbox")[0].as_py()
            minX = bbox["xmin"]
            maxX = bbox["xmax"]
            minY = bbox["ymin"]
            maxY = bbox["ymax"]
            feature = {
                "type": "Feature",
                "id": str(row.column("id")[0]),
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[minX, minY], [maxX, minY], [maxX, maxY], [minX, maxY], [minX, minY]]]
                },
                "properties": {
                    "datetime": row.column("start_datetime")[0].as_py().isoformat(),
                    "url": assets["elevation"]["href"]
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
@usgs3dep.route('/3DEP1M/id/<id>', methods=['GET', 'POST'])
def id_route(id):
    try:
        # execute query
        db = __get_3dep()
        table = db.execute(f"""
            SELECT *
            FROM "3depdb"
            WHERE id == '{id}'
        """).to_arrow_table().slice(0, 1)
        # clean data
        row = {}
        for col in table.column_names:
            val = table.column(col)[0]  # get first row value (Arrow scalar)
            py_val = val.as_py() # convert PyArrow scalar to Python object
            if isinstance(py_val, list):
                row[col] = py_val
            elif isinstance(py_val, (datetime.datetime, datetime.date)):
                row[col] = py_val.isoformat()
            elif not isinstance(py_val, (bytes, bytearray)):
                row[col] = py_val
        # return response
        return json.dumps(row)
    except Exception as e:
        abort(400, f'Failed to query 3DEP metadata service: {e}')
