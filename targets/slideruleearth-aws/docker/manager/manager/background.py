import threading
import time
from datetime import datetime, timedelta
from flask import current_app
from manager.db import export_db

BACKUP_HOUR = 2
BACKUP_MIN = 0

def database_backup():
    while True:
        # Wait until time for backup
        now = datetime.now()
        next_run = datetime.combine(now.date(), datetime.min.time()) + timedelta(hours=BACKUP_HOUR, minutes=BACKUP_MIN)        
        if next_run <= now:
            next_run += timedelta(days=1)  # schedule for the next day
        wait_seconds = (next_run - now).total_seconds()
        time.sleep(wait_seconds)
        # Backup database
        try:
            if current_app.config['REMOTE_DATABASE'] != None:
                export_db()
            else:
                print(f'Skipping export of database, remote database not configured')
        except Exception as e:
            print(f"Error in exporting database: {e}")

def database_backup_run():
    thread = threading.Thread(target=database_backup, daemon=True)
    thread.start()
