from flask import (Blueprint, request, current_app)
from werkzeug.exceptions import abort
from manager.db import execute_command_db, columns_db
from manager.geo import get_geo
from datetime import date, datetime
import json
import requests
import hashlib

####################
# Initialization
####################

gauge_metrics = {}
count_metrics = {}

user_request_count = {} # the number of times this user has made a request
user_request_time = {} # last time this user made a request

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

def ratelimitit(source_ip, source_ip_location):
    # only rate limit public clusters
    if not current_app.config['IS_PUBLIC']:
        return False
    # determine request times
    now = date.today().isocalendar()
    this_request_time = (now.week, now.year)
    last_request_time = user_request_time.get(source_ip_location, (0,0))
    # update request count
    if this_request_time == last_request_time:
        user_request_count[source_ip_location] += 1
    else:
        # initialize (restart) request count
        user_request_count[source_ip_location] = 1
    # update request time
    user_request_time[source_ip_location] = this_request_time
    # check request rate
    if user_request_count[source_ip_location] >= current_app.config['RATELIMIT_WEEKLY_COUNT']:
        user_request_count[source_ip_location] -= current_app.config['RATELIMIT_BACKOFF_COUNT']
        blockit(source_ip) # for a short period of time
        return True # ip is being rate limited
    # nominal return
    return False # no rate limiting

def blockit(source_ip):
    print(f'Requests originating from {source_ip} will be rate limited for {current_app.config['RATELIMIT_BACKOFF_PERIOD']} seconds starting from {datetime.now()}')
    requests.post(current_app.config['ORCHESTRATOR'] + "/discovery/block", data=json.dumps({"address": source_ip, "duration": current_app.config['RATELIMIT_BACKOFF_PERIOD']}))

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
        source_ip = data['source_ip'].split(",")[0]
        account = data['account']
        captureit(endpoint, duration)
        if endpoint not in current_app.config['ENDPOINT_TLM_EXCLUSION']:
            source_ip_hash = hashit(source_ip)
            source_ip_location = locateit(source_ip, f'{client},{endpoint}')
            ratelimitit(source_ip, source_ip_location)
            entry = ( data["record_time"],
                      source_ip_hash,
                      source_ip_location,
                      data['aoi']['x'],
                      data['aoi']['y'],
                      client,
                      endpoint,
                      duration,
                      data['status_code'],
                      account,
                      data['version'] )
            cmd = f"""
                INSERT INTO telemetry ({', '.join(columns_db("telemetry"))})
                VALUES (?, ?, ?, ST_Point(?, ?), ?, ?, ?, ?, ?, ?)
            """
            execute_command_db(cmd, entry)
    except Exception as e:
        abort(400, f'Telemetry record failed to post: {e}')
    return f'Telemetry record successfully posted'
