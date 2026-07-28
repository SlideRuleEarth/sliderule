#
# See sliderule/clients/python/utils/is2gt_gen_db.py for code that creates the database
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import dbutils, validation
import pyarrow.compute
import json
import duckdb

####################
# Initialization
####################

is2gt = Blueprint('is2gt', __name__, url_prefix='/ams')

print("Initializing ICESat-2 Ground Track metadata services...")

####################
# Module Functions
####################

def __get_is2gt():
    if 'is2gtdb' not in g:
        g.is2gtdb = duckdb.connect(current_app.config['IS2GT_DB'], read_only=True)
        g.is2gtdb.execute("LOAD spatial;")
    return g.is2gtdb

def close_is2gt(e=None):
    db = g.pop('is2gtdb', None)
    if db is not None:
        db.close()

def init_app(app):
    app.teardown_appcontext(close_is2gt)

####################
# APIs
####################

#
# is2gt
#
@is2gt.route('/IS2GT', methods=['GET', 'POST'])
@validation.validate
def is2gt_route():
    try:
        data = request.get_json()
        state = {'WHERE': False}
        db = __get_is2gt()
        table = db.execute(f"""
            SELECT
                time,
                ST_X(geometry) AS lon,
                ST_Y(geometry) AS lat
            FROM is2gtdb
            {dbutils.build_radius_query(state, data)}
            {dbutils.build_time_query(state, data)}
            {dbutils.build_polygon_query(state, data)}
        """).to_arrow_table()
        hits = len(table)
        lon = table.column("lon").to_pylist()
        lat = table.column("lat").to_pylist()
        times = [t.as_py().isoformat() + "Z" for t in table.column("time")]
        response = {"hits": hits, "time": times, "lon": lon, "lat": lat}
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ICESat-2 Ground Track metadata service: {e}')

#
# is2gt - legacy
#
@is2gt.route('/IS2GT/legacy', methods=['GET', 'POST'])
@validation.validate
def is2gt_legacy_route():
    try:
        data = {
            "lon": request.args.get("lon", type=float),
            "lat": request.args.get("lat", type=float),
            "r": request.args.get("r", type=float),
            "u": request.args.get("u"),
        }
        state = {'WHERE': False}
        db = __get_is2gt()
        table = db.execute(f"""
            SELECT
                time,
                ST_X(geometry) AS lon,
                ST_Y(geometry) AS lat
            FROM is2gtdb
            {dbutils.build_radius_query(state, data)}
        """).to_arrow_table()
        t = [t.split(" ")[1] for t in table.column("time")] # times
        d = [t.split(" ")[0].split("-") for t in table.column("time")] # dates day-month-year
        d = [f"{int(t[2]):04}-{int(t[1]):02}-{int(t[0]):02}" for t in d] # dates YYYY-MM-DD
        lon = table.column("lon").to_pylist()
        lat = table.column("lat").to_pylist()
        result = [{"date": r[0], "time": r[1], "lon": r[2], "lat": r[3]} for r in zip(d, t, lon, lat)]
        response = {"state": True, "result": result}
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ICESat-2 Ground Track metadata service: {e}')
