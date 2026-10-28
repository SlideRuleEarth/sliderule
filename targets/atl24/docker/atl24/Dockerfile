ARG repo
FROM $repo/sliderule-buildenv:latest AS buildenv

RUN dnf update \
  && dnf install -y \
  libarchive-devel \
  && dnf clean all \
  && rm -rf /var/cache/yum

# build and install xgboost
WORKDIR /
RUN git clone --recurse-submodules https://github.com/dmlc/xgboost.git && \
    cd xgboost && \
    VERSION=$(curl -s https://api.github.com/repos/dmlc/xgboost/releases/latest | jq -r '.tag_name') && \
    git checkout $VERSION && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# build and install sliderule application
COPY sliderule /sliderule
WORKDIR /sliderule/targets/slideruleearth
RUN make config-release INSTALLDIR=/usr/local && make

# build and install atl24 plugin
COPY atl24_v2_algorithms /atl24_v2_algorithms
WORKDIR /sliderule/targets/atl24
RUN make config && make && make install

# install runtime dependencies
FROM amazonlinux:2023 AS runtime
RUN dnf update \
  && dnf install -y \
  readline \
  lua \
  openssl \
  libtiff \
  zlib \
  zstd \
  libarchive \
  && dnf clean all \
  && rm -rf /var/cache/yum

#  gmp \
#  libomp \

# copy sliderule installation from buildenv
COPY --from=buildenv /usr/local /usr/local

# configure any new shared libraries
RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/local.conf && ldconfig

# set entrypoint
COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
