FROM condaforge/mambaforge:4.9.2-5 as conda

COPY conda-linux-aarch64.lock .
RUN mamba create --copy -p /env --file conda-linux-aarch64.lock && \
    conda clean -afy

RUN /env/bin/pip install torch

RUN echo "source activate /env" > ~/.bashrc

COPY pointnet2 /
COPY *.py /usr/local/etc/
