FROM python:3.9-slim-buster

ENV DEBIAN_FRONTEND="noninteractive" TZ="America/Los_Angeles"

RUN useradd --create-home --shell /bin/bash sliderule

RUN apt-get update -y && \
    apt-get install -y \
        build-essential \
        gcc \
        git \
        libproj-dev \
        proj-data \
        proj-bin \
        libgeos-dev && \
    apt-get clean

WORKDIR /home/sliderule

RUN pip3 install --no-cache-dir --no-binary=cartopy \
        cython \
        geopandas \
        ipykernel \
        ipywidgets \
        ipyleaflet \
        matplotlib \
        numpy \
        pandas \
        pyproj \
        requests \
        scipy \
        setuptools_scm \
        shapely \
        cartopy

COPY . .

RUN --mount=source=.git,target=.git,type=bind \
    pip install --no-cache-dir --no-deps .

USER sliderule

CMD ["bash"]
