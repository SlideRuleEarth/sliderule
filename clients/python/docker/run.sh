#!/usr/bin/bash
docker run -it --rm --name=python-app -p 8866:8866 -v /data:/data --entrypoint /usr/local/etc/docker-entrypoint.sh icesat2sliderule/python:latest