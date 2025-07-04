from flask import (Blueprint, request)
from werkzeug.exceptions import abort
from manager.db import execute_command_db, export_db
from manager.telemetry import get_metrics
import json

####################
# Initialization
####################

status = Blueprint('status', __name__, url_prefix='/manager/status')

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
# Build Exclude List
#
def build_exclude_list(exclude_list):
    if exclude_list != None and len(exclude_list) > 0:
        return f'EXCLUDE ({', '.join(exclude_list)})'
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
            result = execute_command_db(cmd).fetchall()
            # return response
            response = {key:value for (key,value) in result}
            return json.dumps(response)
        else:
            raise RuntimeError(f'Unable to count field {field}')
    except Exception as e:
        abort(400, f'Value counts request failed: {e}')

#
# Calculate Time Span
#
def calc_timespan(table, field, valid_fields):
    try:
        # check if valid field to query
        if field in valid_fields:
            # build initial query
            cmd = f"""
                SELECT
                    MIN({field}) AS start_time,
                    MAX({field}) AS end_time
                FROM {table};
            """
            # execute request
            result = execute_command_db(cmd).fetchall()
            # return response
            response = {"start": result[0][0].isoformat(), "end": result[0][1].isoformat(), "span": (result[0][1] - result[0][0]).total_seconds()}
            return json.dumps(response)
        else:
            raise RuntimeError(f'Unable to calculate timespan of {field}')
    except Exception as e:
        abort(400, f'Timespan request failed: {e}')

#
# List Rows
#
def list_rows(table, time_field, exclude_list=None):
    try:
        # build query
        cmd = f"""
            SELECT *
            {build_exclude_list(exclude_list)}
            FROM {table}
            {build_time_range_query(time_field)}
        """
        # execute request
        cursor = execute_command_db(cmd)
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
# Metrics
#
@status.route('/prometheus', methods=['GET'])
def prometheus():
    try:
        gauge_metrics, count_metrics = get_metrics()
        metric_response = []
        for name,value in count_metrics.items():
            metric_response.append(f"""# TYPE {name} counter\n{name} {value}\n""")

        for name,value in gauge_metrics.items():
            metric_response.append(f"""# TYPE {name} gauge\n{name} {value}\n""")

        return '\n'.join(metric_response)
    except Exception as e:
        abort(500, f'Metric request failed: {e}')

#
# Request Counts
#
@status.route('/telemetry_counts/<field>', methods=['GET'])
def telemetry_counts(field):
    return value_counts("telemetry", field, ['source_ip_hash', 'source_ip_location', 'client', 'endpoint', 'status_code', 'account', 'version'], "record_time")

#
# Alarm Counts
#
@status.route('/alert_counts/<field>', methods=['GET'])
def alert_counts(field):
    return value_counts("alerts", field, ['status_code', 'version'], "record_time")

#
# Request List
#
@status.route('/telemetry_list', methods=['GET'])
def telemetry_list():
    return list_rows("telemetry", "record_time", ["aoi"])

#
# Alarm List
#
@status.route('/alert_list', methods=['GET'])
def alert_list():
    return list_rows("alerts", "record_time")

#
# Time Span
#
@status.route('/timespan/<field>', methods=['GET'])
def timespan(field):
    return calc_timespan("telemetry", field, ['record_time'])