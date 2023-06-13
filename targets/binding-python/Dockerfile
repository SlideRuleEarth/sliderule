FROM continuumio/miniconda3
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

# Install system dependencies
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
  libtiff-dev \
  libsqlite3-dev \
  sqlite3 \
  && rm -rf /var/lib/apt/lists/*

# instal proj9 gdal dependency
WORKDIR /
RUN wget http://download.osgeo.org/proj/proj-9.2.0.tar.gz
RUN tar xvfz proj-9.2.0.tar.gz
RUN mkdir -p /proj9/build
WORKDIR /proj9/build
RUN cmake /proj-9.2.0 -DCMAKE_BUILD_TYPE=Release
RUN make -j8
RUN make install
RUN ldconfig

# Environment
ENV PYTHONPATH=/usr/local/lib
RUN conda create --name srpy
SHELL ["conda", "run", "-n", "srpy", "/bin/bash", "-c"]

# Install conda-managed dependencies
RUN conda install -c conda-forge openssl r-uuid libcurl Zlib gdal proj rapidjson pybind11

# Install build dependencies
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
  uuid-dev \
  && rm -rf /var/lib/apt/lists/*

# Copy sliderule source repository
COPY sliderule /sliderule
WORKDIR /sliderule/targets/binding-python
RUN make config-conda && make && make install

# Support interactive mode
CMD ["/bin/bash"]