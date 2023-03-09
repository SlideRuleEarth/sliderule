# install build dependencies
FROM ubuntu:20.04 AS buildenv
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  pkg-config \
  cmake \
  git \
  wget \
  libreadline-dev \
  liblua5.3-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  uuid-dev \
  zlib1g-dev \
  libtiff-dev \
  libsqlite3-dev \
  sqlite3 \
  && rm -rf /var/lib/apt/lists/*

# install rapidjson dependency
WORKDIR /
RUN git clone https://github.com/Tencent/rapidjson.git
RUN mkdir -p /build/rapidjson
WORKDIR /build/rapidjson
RUN cmake /rapidjson
RUN make -j8
RUN make install

# install arrow dependency
WORKDIR /
RUN git clone https://github.com/apache/arrow.git
RUN mkdir -p /build/arrow
WORKDIR /build/arrow
RUN cmake /arrow/cpp -DARROW_PARQUET=ON -DARROW_WITH_ZLIB=ON
RUN make -j8
RUN make install

# instal proj9 gdal dependency
WORKDIR /
RUN wget http://download.osgeo.org/proj/proj-9.2.0.tar.gz
RUN tar xvfz proj-9.2.0.tar.gz
RUN mkdir -p /build/proj9
WORKDIR /build/proj9
RUN cmake /proj-9.2.0 -DCMAKE_BUILD_TYPE=Release
RUN make -j8
RUN make install
RUN ldconfig

# install geos gdal dependency
WORKDIR /
RUN git clone https://github.com/libgeos/geos.git
RUN mkdir -p /build/geos
WORKDIR /build/geos
RUN cmake /geos -DCMAKE_BUILD_TYPE=Release
RUN make -j8
RUN make install
RUN ldconfig

# install gdal dependency
WORKDIR /
RUN wget http://download.osgeo.org/gdal/3.6.2/gdal-3.6.2.tar.gz
RUN tar xvfz gdal-3.6.2.tar.gz
RUN mkdir -p /build/gdal
WORKDIR /build/gdal
RUN cmake /gdal-3.6.2 -DCMAKE_BUILD_TYPE=Release
RUN make -j8
RUN make install
RUN ldconfig

# copy sliderule source repository
COPY sliderule /sliderule

# build and install sliderule into staging
RUN mkdir -p /build/sliderule
WORKDIR /build/sliderule
RUN cmake -DCMAKE_BUILD_TYPE=Release \
          -DMAX_FREE_STACK_SIZE=1 \
          -DUSE_ARROW_PACKAGE=ON \
          -DUSE_AWS_PACKAGE=ON \
          -DUSE_H5_PACKAGE=ON \
          -DUSE_NETSVC_PACKAGE=ON \
          -DUSE_GEO_PACKAGE=ON \
          -DUSE_LEGACY_PACKAGE=OFF \
          -DUSE_CCSDS_PACKAGE=OFF \
          /sliderule
RUN make -j8
RUN make install

# build and install icesat2 plugin into staging
RUN mkdir -p /build/icesat2
WORKDIR /build/icesat2
RUN cmake -DCMAKE_BUILD_TYPE=Release \
          /sliderule/plugins/icesat2
RUN make -j8
RUN make install

# build and install pgc plugin into staging
RUN mkdir -p /build/pgc
WORKDIR /build/pgc
RUN cmake -DCMAKE_BUILD_TYPE=Release \
          /sliderule/plugins/pgc
RUN make -j8
RUN make install

# build and install gedi plugin into staging
RUN mkdir -p /build/gedi
WORKDIR /build/gedi
RUN cmake -DCMAKE_BUILD_TYPE=Release \
          /sliderule/plugins/gedi
RUN make -j8
RUN make install

# install project specific files
COPY etc/* /usr/local/etc/sliderule/

# install runtime dependencies
FROM ubuntu:20.04 AS runtime
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
  libreadline8 \
  liblua5.3 \
  libssl1.1 \
  libtiff5 \
  zlib1g \
  curl \
  && rm -rf /var/lib/apt/lists/*

# copy sliderule installation from buildenv
COPY --from=buildenv /usr/local /usr/local

# configure any new shared libraries
RUN ldconfig

# set entrypoint to bash shell (expect it to be overridden)
ENTRYPOINT ["/bin/sh"]
