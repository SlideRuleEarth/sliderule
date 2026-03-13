#!/bin/bash
/env/bin/gunicorn 'ams:create_app()' --bind 0.0.0.0:9082 --reuse-port # --log-level debug --capture-output
