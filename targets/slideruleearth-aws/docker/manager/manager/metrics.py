from flask import (Blueprint, request, current_app)
from werkzeug.exceptions import abort
from manager.db import get_db
from manager.geo import get_geo
import hashlib

####################
# Initialization
####################

metrics = Blueprint('metrics', __name__, url_prefix='/metrics')

####################
# Helper Functions
####################

def hashit(source_ip):
    data = (current_app.config['SECRET_SALT'] + source_ip).encode('utf-8')
    return hashlib.sha256(data).hexdigest()

def locateit(source_ip):
    try:
        geo_country, geo_city, geo_asn = get_geo()
        country = geo_country.country(source_ip).country.name
        city = geo_city.city(source_ip).city.name
        return f'{country}, {city}'
    except Exception as e:
        print(f'Failed to get location information: {e}')
        return f'unknown, unknown'

def allcolumns(table, db):
    columns = db.execute(f"""
        SELECT column_name
        FROM information_schema.columns
        WHERE table_name = '{table}'
    """).fetchall()
    return [col[0] for col in columns]

####################
# APIs
####################

#
# Record Request
#
@metrics.route('/record_request', methods=['POST'])
def record_request():
    try:
        data = request.get_json()
        entry = ( data["request_time"],
                  hashit(data['source_ip']),
                  locateit(data['source_ip']),
                  data['aoi']['x'],
                  data['aoi']['y'],
                  data['client'],
                  data['endpoint'],
                  data['duration'],
                  data['status_code'],
                  data['account'],
                  data['version'],
                  data['message'] )
        db = get_db()
        columns = allcolumns("requests", db)
        db.execute(f"""
            INSERT INTO requests ({', '.join(columns)})
            VALUES (?, ?, ?, ST_Point(?, ?), ?, ?, ?, ?, ?, ?, ?)
        """, entry)
    except Exception as e:
        abort(400, f'Request record failed to post: {e}')
    return f'Request record successfully posted'

#
# Issue Alarm
#
@metrics.route('/issue_alarm', methods=['POST'])
def issue_alarm():
    try:
        data = request.get_json()
        entry = ( data['alarm_time'],
                  data['status_code'],
                  data['account'],
                  data['version'],
                  data['message'] )
        db = get_db()
        columns = allcolumns("alarms", db)
        db.execute(f"""
            INSERT INTO alarms ({', '.join(columns)})
            VALUES (?, ?, ?, ?, ?)
        """, entry)
    except Exception as e:
        abort(400, f'Alarm record failed to post: {e}')
    return f'Alarm record successfully posted'
