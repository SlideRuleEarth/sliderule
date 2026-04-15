#
# See sliderule/clients/python/utils/casals_gen_db.py for code that creates the metadata database
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

casals1b = Blueprint('casals1b', __name__, url_prefix='/ams')

print("Initializing CASALS1B metadata services...")

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
    app.teardown_appcontext(close_casals1b)

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
        state = {'WHERE': False}
        db = __get_casals1b()
        table = db.execute(f"""
            SELECT *
            FROM casals1bdb
            {dbutils.build_time_query(state, data)}
            {dbutils.build_polygon_query(state, data)}
        """).to_arrow_table()
        hits = len(table)
        granules_column = table.column("granule")
        unique_granules = pyarrow.compute.unique(granules_column)
        response = {"hits": hits, "granules":unique_granules.to_pylist()}
        return json.dumps(response)
    except Exception as e:
        print(f"Exception: {e}")
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
        print(f"Exception: {e}")
        abort(400, f'Failed to query CASALS 1B metadata service: {e}')
