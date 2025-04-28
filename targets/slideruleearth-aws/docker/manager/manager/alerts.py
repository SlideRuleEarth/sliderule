from flask import (Blueprint, request)
from werkzeug.exceptions import abort
from manager.db import get_db, allcolumns

####################
# Initialization
####################

alerts = Blueprint('alerts', __name__, url_prefix='/manager/alerts')

####################
# APIs
####################

#
# Issue
#
@alerts.route('/issue', methods=['POST'])
def issue():
    try:
        data = request.get_json()
        entry = ( data['record_time'],
                  data['status_code'],
                  data['account'],
                  data['version'],
                  data['message'] )
        db = get_db()
        columns = allcolumns("alerts", db)
        db.execute(f"""
            INSERT INTO alerts ({', '.join(columns)})
            VALUES (?, ?, ?, ?, ?)
        """, entry)
    except Exception as e:
        abort(400, f'Alert record failed to post: {e}')
    return f'Alert record successfully posted'
