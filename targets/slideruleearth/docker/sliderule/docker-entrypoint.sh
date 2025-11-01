#!/bin/bash
cp /plugins/*.so /usr/local/etc/sliderule/ 2>/dev/null || true
cp /plugins/api/*.lua /usr/local/etc/sliderule/api/ 2>/dev/null || true
cp /plugins/ext/*.lua /usr/local/etc/sliderule/ext/ 2>/dev/null || true
/usr/local/bin/sliderule /usr/local/etc/sliderule/server.lua /usr/local/etc/sliderule/config.json