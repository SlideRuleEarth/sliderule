#!/bin/bash
/usr/local/bin/promtail -config.file=/usr/local/etc/promtail.yml &
/usr/local/bin/node_exporter --path.procfs=/host/proc --path.sysfs=/host/sys --path.rootfs=/rootfs &
/usr/bin/bash