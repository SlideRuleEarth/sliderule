###########
# BUILDENV
###########
FROM fedora:39 as buildenv

# install build dependencies
RUN  dnf update -y \
  && dnf groupinstall -y "Development Tools" \
  && dnf install -y \
         make \
         automake \
         gcc \
         gcc-c++ \
         cmake \
         cppcheck \
         opencv-devel \
         python3.9  \
         parallel \
         gmp-devel \
         mlpack-devel \
         mlpack-bin \
         gdal-devel \
         armadillo-devel \
#         'dnf-command(config-manager)' \
#  && dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/fedora39/x86_64/cuda-fedora39.repo \
#  && dnf -y install cuda libcudnn8 libcudnn8-devel \
  && dnf clean all \
  && rm -rf /var/cache/yum

# set working directory to root
WORKDIR /

# create python environment
RUN python3.9 -m venv venv \
    && source ./venv/bin/activate \
    && python -m pip install --upgrade pip \
    && pip install torch numpy

    
##########
# RUNTIME
##########
#FROM fedora:39 AS runtime

# install runtime dependencies
#RUN  dnf update -y \
#  && dnf install -y \
#     opencv \
#     python3.9 \
#     parallel \
#     gmp \
#     mlpack-bin \
#     gdal \
#     armadillo \
#     'dnf-command(config-manager)' \
#  && dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/fedora39/x86_64/cuda-fedora39.repo \
#  && dnf -y install cuda libcudnn8 libcudnn8-devel \
#  && dnf clean all \
#  && rm -rf /var/cache/yum

# copy sliderule installation from buildenv
#COPY --from=buildenv /usr/local /usr/local

# configure any new shared libraries
#RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/local.conf && ldconfig

# copy runtime scripts
COPY surface.sh /surface.sh
COPY bathy.sh /bathy.sh

# set entrypoint to bash shell (expect it to be overridden)
ENTRYPOINT ["/bin/sh"]