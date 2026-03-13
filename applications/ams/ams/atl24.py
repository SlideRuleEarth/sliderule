#
# See sliderule-atl24/utils/gen_db.py for code that creates the metadata database
#

from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import dbutils
import pyarrow.compute
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
    app.teardown_appcontext(close_atl24)

####################
# APIs
####################

#
# ATL24
#
@atl24.route('/ATL24', methods=['GET', 'POST'])
def atl24_route():
    try:
        # execute query
        data = request.get_json()
        name_filter = dbutils.get_icesat2_name_filter(data)
        state = {'WHERE': False}
        db = __get_atl24()
        table = db.execute(f"""
            SELECT *
            FROM atl24db
            {dbutils.build_time_query(state, data)}
            {dbutils.build_matching_query(state, data, "season", "season")}
            {dbutils.build_range_query(state, data, "bathy_photons", "photons0", "photons1")}
            {dbutils.build_range_query(state, data, "bathy_mean_depth", "meandepth0", "meandepth1")}
            {dbutils.build_range_query(state, data, "bathy_min_depth", "mindepth0", "mindepth1")}
            {dbutils.build_range_query(state, data, "bathy_max_depth", "maxdepth0", "maxdepth1")}
            {dbutils.build_name_filter(state, name_filter)}
            {dbutils.build_polygon_query(state, data)}
        """).to_arrow_table()
        hits = len(table)
        if hits > current_app.config['MAX_RESOURCES']:
            raise RuntimeError(f"request exceeded maximum number of resources allowed - {hits}")
        else:
            granules_column = table.column("granule")
            unique_granules = pyarrow.compute.unique(granules_column)
            response = {"hits": hits, "granules":unique_granules.to_pylist()}
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL24 metadata service: {e}')

#
# Granule
#
@atl24.route('/ATL24/granule/<name>', methods=['GET', 'POST'])
def granule_route(name):
    try:
        # execute query
        db = __get_atl24()
        table = db.execute(f"""
            SELECT *
            FROM atl24db
            WHERE granule == '{name}'
        """).to_arrow_table()
        # build response
        response = {}
        for i in range(len(table)):
            row = table.slice(i, i+1)
            beam = str(row.column("beam")[0])
            response[beam] = {
                "season": int(row.column("season")[0]),
                "bathy_photons": int(row.column("bathy_photons")[0]),
                "bathy_mean_depth": float(row.column("bathy_mean_depth")[0]),
                "bathy_min_depth": float(row.column("bathy_min_depth")[0]),
                "bathy_max_depth": float(row.column("bathy_max_depth")[0]),
                "begin_time": row.column("begin_time")[0].as_py().isoformat()
            }
        # return response
        return json.dumps(response)
    except Exception as e:
        abort(400, f'Failed to query ATL24 metadata service: {e}')
