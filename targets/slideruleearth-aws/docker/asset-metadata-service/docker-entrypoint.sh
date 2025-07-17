#!/bin/bash
/env/bin/gunicorn 'ams:create_app()' --bind 0.0.0.0:9082 # --log-level debug
