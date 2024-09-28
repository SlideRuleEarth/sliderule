#
# RUNTIME
#
FROM fedora:39

# runtime dependencies
RUN dnf update -y \
    && dnf install -y \
        wget \
    && dnf clean all \
    && rm -rf /var/cache/yum

#
#   The following section builds a python environment to run the python applications in
#   oceaneyes.  This environment is not being used because pointnet2 runs substantially
#   faster using a special environment that is currently unable to be replicated
#
# # install mamba for python based algorithms
# RUN wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh \
#     && bash Miniforge3-Linux-aarch64.sh -b
# 
# # install python dependencies for python based algorithms
# COPY conda-linux-aarch64.lock .
# RUN /root/miniforge3/bin/mamba create --copy -p /env --file conda-linux-aarch64.lock \
#     && /root/miniforge3/bin/conda clean -afy
# RUN /env/bin/pip install fast_histogram

# install magic pytorch environment
COPY magic_pytorch_env /magic

# install python applications
COPY runner.sh /runner.sh
COPY runner.py /runner.py
COPY bathypathfinder /bathypathfinder
COPY cshelph /cshelph
COPY medianfilter /medianfilter
COPY openoceans /openoceans
COPY pointnet /pointnet

# container defaults
SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]