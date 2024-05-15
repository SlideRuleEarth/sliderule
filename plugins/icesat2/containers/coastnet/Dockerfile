FROM fedora:39 as buildenv

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
  && dnf clean all \
  && rm -rf /var/cache/yum

WORKDIR /

RUN python3.9 -m venv venv \
    && source ./venv/bin/activate \
    && python -m pip install --upgrade pip \
    && pip install torch numpy

COPY ut-coastnet /
COPY CMakeLists.txt /ut-coastnet/
COPY Makefile /ut-coastnet/
COPY surface.sh /
COPY bathy.sh /

RUN source /venv/bin/activate \
    && cd /ut-coastnet \
    && make

# Set the SHELL to Bash
SHELL ["/bin/bash", "-c"]

# Default command when the container starts
CMD ["/bin/bash"]