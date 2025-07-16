#!/bin/bash
if [ -z "$DUCKDB_LOCAL_FILE" ] || [ ! -e "$DUCKDB_LOCAL_FILE" ]; then
    echo "Creating local duckdb database: $DUCKDB_LOCAL_FILE"
    /env/bin/flask --app manager init-db
fi
/env/bin/gunicorn 'manager:create_app()' --bind 0.0.0.0:8000 # --log-level debug
