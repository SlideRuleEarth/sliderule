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
         clang \
         llvm \
         cmake \
         cppcheck \
         opencv-devel \
         python3.9  \
         parallel \
         gmp-devel \
         libomp-devel \
         mlpack-devel \
         mlpack-bin \
         gdal-devel \
         armadillo-devel \
  && dnf clean all \
  && rm -rf /var/cache/yum

# work at the root
WORKDIR /

# python build environment
RUN python3.9 -m venv venv \
    && source ./venv/bin/activate \
    && python -m pip install --upgrade pip \
    && pip install torch numpy pandas

# build coastnet
COPY coastnet /coastnet
RUN source /venv/bin/activate \
    && cd /coastnet \
    && CC=clang CXX=clang++ make

#
# RUNTIME
#
FROM fedora:39 AS runtime

# coastnet runtime dependencies
RUN dnf update -y && \
    dnf install -y \
      wget \
      opencv \
      python3.9  \
      gmp \
      libomp \
      mlpack-bin \
      gdal \
      armadillo && \
    dnf clean all && \
    rm -rf /var/cache/yum

# coastnet python dependencies
RUN python3.9 -m venv venv && \
    source ./venv/bin/activate && \
    python -m pip install --upgrade pip && \
    pip install torch numpy pandas

# install coastnet binaries (from build environment above)
COPY --from=buildenv /coastnet /coastnet

# install mamba for python based algorithms
RUN wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh \
    && bash Miniforge3-Linux-aarch64.sh -b

# install python dependencies for python based algorithms
COPY conda-linux-aarch64.lock .
RUN /root/miniforge3/bin/mamba create --copy -p /env --file conda-linux-aarch64.lock \
    && /root/miniforge3/bin/conda clean -afy
RUN /env/bin/pip install torch fast_histogram

# install applications
COPY coastnet/surface.sh /coastnet
COPY coastnet/bathy.sh /coastnet
COPY coastnet/*.py /coastnet
COPY atl24writer /atl24writer
COPY bathypathfinder /bathypathfinder
COPY cshelph /cshelph
COPY medianfilter /medianfilter
COPY openoceans /openoceans
COPY pointnet2 /pointnet2

# container defaults
RUN echo "source activate /env" > ~/.bashrc
SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]