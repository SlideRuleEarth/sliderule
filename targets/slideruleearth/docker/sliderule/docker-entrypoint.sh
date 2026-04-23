#!/bin/bash
cp /plugins/* /usr/local/etc/sliderule/ 2>/dev/null || true
cp /plugins/api/* /usr/local/etc/sliderule/api/ 2>/dev/null || true
cp /plugins/ext/* /usr/local/etc/sliderule/ext/ 2>/dev/null || true
/usr/local/bin/sliderule "$@"