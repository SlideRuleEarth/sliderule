#!/bin/bash
cp /plugins/*.so /usr/local/etc/sliderule/ || true
cp /plugins/api/*.lua /usr/local/etc/sliderule/api/ || true
cp /plugins/ext/*.lua /usr/local/etc/sliderule/ext/ || true
/usr/local/bin/sliderule /usr/local/etc/sliderule/server.lua /usr/local/etc/sliderule/config.json