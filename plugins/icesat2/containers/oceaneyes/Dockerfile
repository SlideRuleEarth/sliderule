FROM condaforge/mambaforge:4.9.2-5 as conda

COPY conda-linux-aarch64.lock .
RUN mamba create --copy -p /env --file conda-linux-aarch64.lock && \
    conda clean -afy

RUN /env/bin/pip install torch fast_histogram

RUN echo "source activate /env" > ~/.bashrc

COPY atl24writer /atl24writer
COPY bathypathfinder /bathypathfinder
COPY cshelph /cshelph
COPY medianfilter /medianfilter
COPY openoceans /openoceans
COPY pointnet2 /pointnet2
