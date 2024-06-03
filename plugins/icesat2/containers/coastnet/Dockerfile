#
# BUILDENV
#
FROM fedora:39 as buildenv

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

WORKDIR /

RUN python3.9 -m venv venv \
    && source ./venv/bin/activate \
    && python -m pip install --upgrade pip \
    && pip install torch numpy pandas

COPY coastnet /
COPY CMakeLists.txt /coastnet/
COPY Makefile /coastnet/
RUN source /venv/bin/activate \
    && cd /coastnet \
    && CC=clang CXX=clang++ make

#
# RUNTIME
#
FROM fedora:39 AS runtime

RUN  dnf update -y \
  && dnf install -y \
         opencv \
         python3.9  \
         gmp \
         libomp \
         mlpack-bin \
         gdal \
         armadillo \
  && dnf clean all \
  && rm -rf /var/cache/yum

COPY --from=buildenv /coastnet /coastnet

RUN python3.9 -m venv venv \
    && source ./venv/bin/activate \
    && python -m pip install --upgrade pip \
    && pip install torch numpy pandas

COPY surface.sh /coastnet
COPY bathy.sh /coastnet
COPY *.py /coastnet

SHELL ["/bin/bash", "-c"]

CMD ["/bin/bash"]