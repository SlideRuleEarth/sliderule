ARG repo
FROM $repo/sliderule-buildenv:latest AS buildenv

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

RUN wget https://repo.anaconda.com/miniconda/Miniconda3-py310_23.3.1-0-Linux-aarch64.sh
RUN chmod +x Miniconda3-py310_23.3.1-0-Linux-aarch64.sh
RUN ./Miniconda3-py310_23.3.1-0-Linux-aarch64.sh -b

# Make RUN commands use the new environment:
RUN conda init bash
SHELL ["conda", "run", "-n", "bash", "/bin/bash", "-c"]
RUN conda install -c conda-forge readline lua openssl r-uuid libcurl Zlib gdal pybind11

# copy sliderule installation from buildenv
COPY --from=buildenv /usr/local /usr/local

# configure any new shared libraries
RUN ldconfig

# set entrypoint to bash shell (expect it to be overridden)
ENTRYPOINT ["/bin/sh"]
