from flask import (Blueprint, request)
from werkzeug.exceptions import abort
from manager.db import execute_command_db, columns_db

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
                  data['version'],
                  data['message'] )
        cmd = f"""
            INSERT INTO alerts ({', '.join(columns_db("alerts"))})
            VALUES (?, ?, ?, ?)
        """
        execute_command_db(cmd, entry)
    except Exception as e:
        abort(400, f'Alert record failed to post: {e}')
    return f'Alert record successfully posted'
