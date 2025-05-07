import duckdb
import click
from flask import current_app, g
import threading

def __getdb():
    if 'db' not in g:
        g.db = duckdb.connect(current_app.config['DATABASE'])
        g.db.execute("""
            INSTALL spatial;
            LOAD spatial;
        """)
        g.lock = threading.Lock()
    return g.db, g.lock

def columns_db(table):
    db, lock = __getdb()
    column_attr = f'{table}_columns'
    if column_attr not in g:
        with lock:
            columns = db.execute(f"""
                SELECT column_name
                FROM information_schema.columns
                WHERE table_name = '{table}'
            """).fetchall()
            setattr(g, column_attr, [col[0] for col in columns])
    return getattr(g, column_attr)

def execute_file_db(file):
    db, lock = __getdb()
    with current_app.open_resource(file) as f:
        with lock:
            return db.execute(f.read().decode('utf8'))

def execute_command_db(cmd, entry=None):
    db, lock = __getdb()
    with lock:
        return db.execute(cmd, entry)

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
    app.teardown_appcontext(close_db)
    app.cli.add_command(init_db_command)
