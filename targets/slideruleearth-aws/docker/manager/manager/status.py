from flask import (Blueprint, g, request)
from werkzeug.exceptions import abort
from manager.db import get_db
import json

####################
# Initialization
####################

status = Blueprint('status', __name__, url_prefix='/status')

####################
# Helper Functions
####################

#
# Build Time Range Query
#
def build_time_range_query(time_field):
    duration = request.args.get('duration')
    t0 = request.args.get('t0')
    t1 = request.args.get('t1')
    if duration != None and int(duration) > 0:
        return f'WHERE {time_field} >= NOW() - INTERVAL {duration} seconds'
    elif t0 != None and t1 != None:
        return f'WHERE {time_field} BETWEEN {t0} AND {t1}'
    elif t0 != None:
        return f'WHERE {time_field} >= {t0}'
    elif t1 != None:
        return f'WHERE {time_field} <= {t1}'
    else:
        return ''

#
# Value Counts
#
def value_counts(table, field, valid_fields, time_field):
    try:
        # check if valid field to query
        if field in valid_fields:
            # build initial query
            cmd = f"""
                SELECT {field}, COUNT(*) AS count
                FROM {table}
                {build_time_range_query(time_field)}
                GROUP BY {field}
            """
            # execute request
            db = get_db()
            result = db.execute(cmd).fetchall()
            # return response
            response = {key:value for (key,value) in result}
            return json.dumps(response)
        else:
            raise RuntimeError(f'Unable to count field {field}')
    except Exception as e:
        abort(400, f'Value counts request failed: {e}')

#
# List Rows
#
def list_rows(table, time_field):
    try:
        # build query
        cmd = f"""
            SELECT * EXCLUDE (aoi)
            FROM {table}
            {build_time_range_query(time_field)}
        """
        # execute request
        db = get_db()
        cursor = db.execute(cmd)
        result = cursor.fetchall()
        # return response
        columns = [desc[0] for desc in cursor.description]
        response = [dict(zip(columns, row)) for row in result]
        return json.dumps(response, default=str)
    except Exception as e:
        abort(400, f'Request list request failed: {e}')

####################
# APIs
####################

#
# Request Counts
#
@status.route('/request_counts/<field>', methods=['GET'])
def request_counts(field):
    return value_counts("requests", field, ['source_ip', 'client', 'endpoint', 'status_code', 'organization', 'version'], "request_time")

#
# Alarm Counts
#
@status.route('/alarm_counts/<field>', methods=['GET'])
def alarm_counts(field):
    return value_counts("alarms", field, ['status_code', 'organization', 'version'], "alarm_time")

#
# Request List
#
@status.route('/request_list', methods=['GET'])
def request_list():
    return list_rows("requests", "request_time")

#
# Alarm List
#
@status.route('/alarm_list', methods=['GET'])
def alarm_list():
    return list_rows("alarms", "alarm_time")