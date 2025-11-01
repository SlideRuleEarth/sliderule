#!/bin/bash
service nginx start
cd /jekyll && bundle exec jekyll serve -d /srv/jekyll/_site --host=0.0.0.0 --skip-initial-build