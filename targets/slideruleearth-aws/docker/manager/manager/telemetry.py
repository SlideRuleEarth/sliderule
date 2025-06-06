from flask import (Blueprint, request, current_app)
from werkzeug.exceptions import abort
from manager.db import execute_command_db, columns_db
from manager.geo import get_geo
import hashlib

####################
# Initialization
####################

gauge_metrics = {}
count_metrics = {}

telemetry = Blueprint('telemetry', __name__, url_prefix='/manager/telemetry')

####################
# Helper Functions
####################

def hashit(source_ip):
    data = (current_app.config['SECRET_SALT'] + source_ip).encode('utf-8')
    return hashlib.sha256(data).hexdigest()

def locateit(source_ip, debug_info):
    try:
        if source_ip == "0.0.0.0" or source_ip == "127.0.0.1":
            return f'localhost, localhost'
        else:
            geo_country, geo_city, geo_asn = get_geo()
            country = geo_country.country(source_ip).country.name
            city = geo_city.city(source_ip).city.name
            return f'{country}, {city}'
    except Exception as e:
        print(f'Failed to get location information for <{debug_info}>: {e}')
        return f'unknown, unknown'

def captureit(endpoint, duration):
    global gauge_metrics, count_metrics
    if type(endpoint) != str:
        raise RuntimeError('endpoint not provided: {endpoint}')
    elif len(endpoint) == 0 or len(endpoint) > 32:
        raise RuntimeError('endpoint name too long: {endpoint}')
    elif not endpoint.isidentifier():
        raise RuntimeError('invalid endpoint name: {endpoint}')
    else:
        gauge_metrics[endpoint] = duration
        gauge_metrics[endpoint + "_sum"] = gauge_metrics.get(endpoint + "_sum", 0.0) + duration
        count_metrics[endpoint + "_count"] = count_metrics.get(endpoint + "_count", 0) + 1



def get_metrics():
    global gauge_metrics, count_metrics
    return gauge_metrics, count_metrics

####################
# APIs
####################

#
# Record
#
@telemetry.route('/record', methods=['POST'])
def record():
    try:
        data = request.get_json()
        client = data['client']
        endpoint = data['endpoint']
        duration = data['duration']
        captureit(endpoint, duration)
        if endpoint not in current_app.config['ENDPOINT_TLM_EXCLUSION']:
            entry = ( data["record_time"],
                      hashit(data['source_ip']),
                      locateit(data['source_ip'], f'{client},{endpoint}'),
                      data['aoi']['x'],
                      data['aoi']['y'],
                      client,
                      endpoint,
                      duration,
                      data['status_code'],
                      data['account'],
                      data['version'] )
            cmd = f"""
                INSERT INTO telemetry ({', '.join(columns_db("telemetry"))})
                VALUES (?, ?, ?, ST_Point(?, ?), ?, ?, ?, ?, ?, ?)
            """
            execute_command_db(cmd, entry)
    except Exception as e:
        abort(400, f'Telemetry record failed to post: {e}')
    return f'Telemetry record successfully posted'
