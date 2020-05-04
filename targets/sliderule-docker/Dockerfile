FROM ubuntu:18.04
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  libreadline7 \
  && rm -rf /var/lib/apt/lists/*

COPY . /usr/local

RUN ldconfig

ENTRYPOINT ["/usr/local/bin/sliderule"]

