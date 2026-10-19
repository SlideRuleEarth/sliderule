#
# See sliderule-atl03/utils/gen_db.py for code that creates the metadata database
#

from flask import (Blueprint, current_app, g)
from werkzeug.exceptions import abort
from . import validation
import json
import duckdb

####################
# Initialization
####################

atl03 = Blueprint('atl03', __name__, url_prefix='/ams')

print("Initializing ATL03 metadata services...")

####################
# Module Functions
####################

def __get_atl03():
    if 'atl03db' not in g:
        g.atl03db = duckdb.connect(current_app.config['ATL03_DB'], read_only=True)
        g.atl03db.execute("LOAD spatial;")
    return g.atl03db

def close_atl03(e=None):
    db = g.pop('atl03db', None)
    if db is not None:
        db.close()

def init_app(app):
    app.teardown_appcontext(close_atl03)

####################
# APIs
####################

#
# Geolocation Correction
#
@atl03.route('/ATL03/geoloc_corr/<name>', methods=['GET', 'POST'])
@validation.validate
def geoloc_corr_route(name):
    try:
        # best effort conversion to ATL03 granule name
        granule = "ATL03" + name[5:36] + ".h5"
        # execute query
        db = __get_atl03()
        table = db.execute("""
            SELECT *
            FROM atl03db
            WHERE granule == ?
              AND shift_N_m IS NOT NULL
              AND shift_E_m IS NOT NULL
              AND shift_U_m IS NOT NULL
        """, [granule]).to_arrow_table()
        # build response
        response = {}
        for row in table.to_pylist():
            response[row["beam"]] = {
                "shift_N_m": row["shift_N_m"],
                "shift_E_m": row["shift_E_m"],
                "shift_U_m": row["shift_U_m"]
            }
        # return response
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
        abort(400, f'Failed to query ATL03 metadata service: {e}')
