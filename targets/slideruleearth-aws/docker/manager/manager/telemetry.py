from flask import (Blueprint, request, current_app)
from werkzeug.exceptions import abort
from manager.db import get_db, allcolumns
from manager.geo import get_geo
import hashlib

####################
# Initialization
####################

telemetry = Blueprint('telemetry', __name__, url_prefix='/manager/telemetry')

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
        entry = ( data["record_time"],
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
        columns = allcolumns("telemetry", db)
        db.execute(f"""
            INSERT INTO telemetry ({', '.join(columns)})
            VALUES (?, ?, ?, ST_Point(?, ?), ?, ?, ?, ?, ?, ?, ?)
        """, entry)
    except Exception as e:
        abort(400, f'Telemetry record failed to post: {e}')
    return f'Telemetry record successfully posted'
