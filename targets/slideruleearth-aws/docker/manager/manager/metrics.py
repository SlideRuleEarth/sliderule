from flask import (Blueprint, g, request)
from werkzeug.exceptions import abort

from supervisor.db import get_db

metrics = Blueprint('metrics', __name__, url_prefix='/metrics')

@metrics.route('/record_request', methods=['POST'])
def record_request():
    try:
        data = request.get_json()
        entry = ( data["request_time"],
                  data['source_ip'],
                  data['aoi']['x'],
                  data['aoi']['y'],
                  data['client'],
                  data['endpoint'],
                  data['duration'],
                  data['status_code'],
                  data['organization'],
                  data['version'],
                  data['message'] )
        db = get_db()
        db.execute("INSERT INTO requests (request_time, source_ip, aoi, client, endpoint, duration, status_code, organization, version, message) VALUES (?, ?, ST_Point(?, ?), ?, ?, ?, ?, ?, ?, ?)", entry)
    except Exception as e:
        abort(400, f'Request record failed to post: {e}')
    return f'Request record successfully posted'

@metrics.route('/issue_alarm', methods=['POST'])
def issue_alarm():
    try:
        data = request.get_json()
        entry = ( data['alarm_time'],
                  data['status_code'],
                  data['organization'],
                  data['version'],
                  data['message'] )
        db = get_db()
        db.execute("INSERT INTO alarms (alarm_time, status_code, organization, version, message) VALUES (?, ?, ?, ?, ?)", entry)
    except Exception as e:
        abort(400, f'Alarm record failed to post: {e}')
    return f'Alarm record successfully posted'
