#
# COASTNET BUILDENV
#
FROM fedora:39 as buildenv

# build dependencies for coastnet
RUN  dnf update -y \
  && dnf groupinstall -y "Development Tools" \
  && dnf install -y \
         make \
         automake \
         gcc-c++ \
         cmake \
         parallel \
         gmp-devel \
         libomp-devel \
         gdal-devel \
         armadillo-devel \
  && dnf clean all \
  && rm -rf /var/cache/yum

# work at the root
WORKDIR /

# development environment for coastnet and qtrees
RUN git clone --recursive https://github.com/dmlc/xgboost \
    && cd xgboost \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j8 \
    && make install \
    && ldconfig

# build qtrees
COPY qtrees /qtrees
RUN cd /qtrees \
    && make

# build coastnet
COPY coastnet /coastnet
RUN cd /coastnet \
    && make

#
# RUNTIME
#
FROM fedora:39 AS runtime

# runtime dependencies
RUN dnf update -y && \
    dnf install -y \
      wget \
      gmp \
      libomp \
      gdal \
      armadillo && \
    dnf clean all && \
    rm -rf /var/cache/yum

# install mamba for python based algorithms
RUN wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh \
    && bash Miniforge3-Linux-aarch64.sh -b

# install python dependencies for python based algorithms
COPY conda-linux-aarch64.lock .
RUN /root/miniforge3/bin/mamba create --copy -p /env --file conda-linux-aarch64.lock \
    && /root/miniforge3/bin/conda clean -afy
RUN /env/bin/pip install fast_histogram

# install magic pytorch environment
COPY magic_pytorch_env /magic

# install binaries (from build environment above)
COPY --from=buildenv /coastnet /coastnet
COPY --from=buildenv /qtrees /qtrees
COPY --from=buildenv /usr/local /usr/local

# install applications
COPY atl24refraction /atl24refraction
COPY atl24uncertainty /atl24uncertainty
COPY atl24writer /atl24writer
COPY bathypathfinder /bathypathfinder
COPY cshelph /cshelph
COPY medianfilter /medianfilter
COPY openoceans /openoceans
COPY pointnet2 /pointnet2
COPY utils /utils

# container defaults
SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]