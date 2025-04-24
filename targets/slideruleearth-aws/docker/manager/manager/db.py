import duckdb
import click
from flask import current_app, g

def get_db():
    if 'db' not in g:
        g.db = duckdb.connect(current_app.config['DATABASE'])
        g.db.execute("""
            INSTALL spatial;
            LOAD spatial;
        """)
    return g.db

def execute_db(file):
    db = get_db()
    with current_app.open_resource(file) as f:
        db.execute(f.read().decode('utf8'))

def close_db(e=None):
    db = g.pop('db', None)
    if db is not None:
        db.close()

@click.command('init-db')
def init_db_command():
    """Clear the existing data and create new tables."""
    execute_db('schema.sql')
    click.echo('Initialized the database.')

def init_app(app):
    app.teardown_appcontext(close_db)
    app.cli.add_command(init_db_command)

