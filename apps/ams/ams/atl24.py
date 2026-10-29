#
# See sliderule-atl24/utils/gen_db.py for code that creates the metadata database
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

atl24 = Blueprint('atl24', __name__, url_prefix='/ams')

print("Initializing ATL24 metadata services...")

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
@validation.validate
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
            {dbutils.build_range_query(state, data, "class_bathymetry", "photons0", "photons1")}
            {dbutils.build_range_query(state, data, "bathy_depth_mean", "meandepth0", "meandepth1")}
            {dbutils.build_range_query(state, data, "bathy_depth_min", "mindepth0", "mindepth1")}
            {dbutils.build_range_query(state, data, "bathy_depth_max", "maxdepth0", "maxdepth1")}
            {dbutils.build_name_filter(state, name_filter)}
            {dbutils.build_polygon_query(state, data)}
        """).to_arrow_table()
        hits = len(table)
        granules_column = table.column("name")
        unique_granules = pyarrow.compute.unique(granules_column)
        response = {"hits": hits, "granules":unique_granules.to_pylist()}
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ATL24 metadata service: {e}')

#
# Granule
#
@atl24.route('/ATL24/granule/<name>', methods=['GET', 'POST'])
@validation.validate
def granule_route(name):
    try:
        # execute query
        db = __get_atl24()
        table = db.execute("""
            SELECT beam, season, class_bathymetry, bathy_depth_mean, bathy_depth_min, bathy_depth_max, begin_time
            FROM atl24db
            WHERE name = ?
        """, [name]).to_arrow_table()
        # build response
        response = {}
        for row in table.to_pylist():
            response[row["beam"]] = {
                "season": row["season"],
                "bathy_photons": row["class_bathymetry"],
                "bathy_mean_depth": row["bathy_depth_mean"],
                "bathy_min_depth": row["bathy_depth_min"],
                "bathy_max_depth": row["bathy_depth_max"],
                "begin_time": row["begin_time"].isoformat() + "Z"
            }
        # return response
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ATL24 metadata service: {e}')
