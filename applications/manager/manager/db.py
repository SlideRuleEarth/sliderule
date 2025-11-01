import duckdb
import click
from flask import Blueprint, current_app, request, g
from werkzeug.exceptions import abort
import boto3
import os

####################
# Initialization
####################

db = Blueprint('db', __name__, url_prefix='/manager/db')

####################
# Helper Functions
####################

def __getdb():
    if 'db' not in g:
        g.db = duckdb.connect(current_app.config['DATABASE'])
        g.db.execute("LOAD spatial;")
    return g.db

####################
# Module Functions
####################

def columns_db(table):
    db = __getdb()
    column_attr = f'{table}_columns'
    if column_attr not in g:
        columns = db.execute(f"""
            SELECT column_name
            FROM information_schema.columns
            WHERE table_name = '{table}'
        """).fetchall()
        setattr(g, column_attr, [col[0] for col in columns])
    return getattr(g, column_attr)

def execute_file_db(file):
    db = __getdb()
    with current_app.open_resource(file) as f:
        return db.execute(f.read().decode('utf8'))

def execute_command_db(cmd, entry=None):
    db = __getdb()
    return db.execute(cmd, entry)

def export_db():
    s3 = boto3.client('s3')
    local_path = current_app.config['DATABASE']
    remote_path = current_app.config['REMOTE_DATABASE'].split("s3://")[-1]
    bucket_name = remote_path.split("/")[0]
    key = '/'.join(remote_path.split("/")[1:])
    s3.upload_file(local_path, bucket_name, key)
    return f's3://{bucket_name}/{key}'

def close_db(e=None):
    db = g.pop('db', None)
    if db is not None:
        db.close()

@click.command('init-db')
def init_db_command():
    """Clear the existing data and create new tables."""
    execute_file_db('schema.sql')
    click.echo('Initialized the database.')

def init_app(app):
    db = duckdb.connect(app.config['DATABASE'])
    try:
        db.execute("INSTALL spatial;")
    except Exception as e:
        print(f"Unable to load spatial extension, assuming it's already loaded: {e}")
    db.close()
    app.teardown_appcontext(close_db)
    app.cli.add_command(init_db_command)

####################
# APIs
####################

#
# Export Database
#
@db.route('/export', methods=['POST'])
def export():
    try:
        api_key = request.headers.get("x-sliderule-api-key")
        if api_key != current_app.config['API_KEY']:
            raise RuntimeError(f'Invalid api key')
        remote_file = export_db()
        return f'Database successfully exported to: {remote_file}'
    except Exception as e:
        abort(500, f'Export failed: {e}')
