FROM ubuntu:20.04
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  libreadline8 \
  liblua5.3 \
  libssl1.1 \
  zlib1g \
  curl \
  && rm -rf /var/lib/apt/lists/*

COPY . /usr/local

RUN ldconfig

ENTRYPOINT ["/usr/local/bin/sliderule"]

