from flask import (Blueprint, request, current_app, g)
from werkzeug.exceptions import abort
from . import icesat2
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
# APIs
####################

#
# ATL24
#
@atl24.route('/atl24', methods=['GET', 'POST'])
def atl24_route():
    try:
        # execute query
        data = request.get_json()
        poly = icesat2.get_polygon_query(data)
        name_filter = icesat2.get_name_filter(data)
        state = {'WHERE': False}
        db = __get_atl24()
        cmd = f"""
            SELECT *
            FROM atl24db
            {icesat2.build_time_query(state, data)}
            {icesat2.build_matching_query(state, data, "season", "season")}
            {icesat2.build_range_query(state, data, "bathy_photons", "photons0", "photons1")}
            {icesat2.build_range_query(state, data, "bathy_mean_depth", "meandepth0", "meandepth1")}
            {icesat2.build_range_query(state, data, "bathy_min_depth", "mindepth0", "mindepth1")}
            {icesat2.build_range_query(state, data, "bathy_max_depth", "maxdepth0", "maxdepth1")}
            {icesat2.build_name_filter(state, name_filter)}
            {icesat2.build_polygon_query(state, poly)}
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
@atl24.route('/atl24/granule/<name>', methods=['GET', 'POST'])
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
