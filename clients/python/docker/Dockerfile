FROM continuumio/miniconda3
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

# environment
ENV PYTHONPATH=/usr/local/lib

# sliderule-python client
ADD "https://api.github.com/repos/ICESat2-SlideRule/sliderule-python/commits?per_page=1" latest_commit
RUN git clone https://github.com/ICESat2-SlideRule/sliderule-python.git && \
    cd sliderule-python && \
    conda env create -f environment.yml

# Make RUN commands use the new environment:
SHELL ["conda", "run", "-n", "sliderule", "/bin/bash", "-c"]
RUN conda install -c conda-forge voila

# entry point
COPY docker-entrypoint.sh /usr/local/etc/
ENTRYPOINT ["/bin/bash"]