#!/bin/bash
grafana-server --homepath "/grafana" --config "/grafana/conf/grafana.ini" web &
prometheus --config.file=/prometheus/prometheus.yml --web.console.templates=/prometheus/consoles --web.console.libraries=/prometheus/console_libraries --web.listen-address=0.0.0.0:9090 &
loki-linux-amd64 -config.file=/loki/loki.yml &
/usr/bin/bash