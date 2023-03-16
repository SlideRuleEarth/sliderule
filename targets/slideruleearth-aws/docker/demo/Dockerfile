FROM continuumio/miniconda3
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

# Environment
ENV PYTHONPATH=/usr/local/lib

# Install SlideRule Python Environment
COPY sliderule-python /sliderule-python
RUN cd /sliderule-python && conda env create -f environment.yml

# Make RUN commands use the new environment:
SHELL ["conda", "run", "-n", "sliderule", "/bin/bash", "-c"]

# Install SlideRule Python Client
COPY sliderule /sliderule
RUN cd /sliderule/clients/python && python setup.py install

# Install Voila
RUN conda install -c conda-forge voila

# Install latest ipyleaflet
RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  build-essential \
  && rm -rf /var/lib/apt/lists/*
RUN conda install -c conda-forge -y yarn jupyter_packaging geopandas
RUN cd /tmp && git clone https://github.com/jupyter-widgets/ipyleaflet.git
RUN cd /tmp/ipyleaflet && \
    pip install -e . && \
    jupyter labextension develop . --overwrite
RUN cd /tmp/ipyleaflet/js && \
    yarn add webpack webpack-dev-server svg-url-loader --dev && \
    yarn run build

# Entry point
COPY docker-entrypoint.sh /usr/local/etc/
ENTRYPOINT ["/bin/bash"]