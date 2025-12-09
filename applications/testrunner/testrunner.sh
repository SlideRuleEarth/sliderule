#!/bin/bash
exec > testrunner.log 2>&1
CONTAINER_REGISTRY=$1
VERSION=$2

#
# System Dependencies
#
dnf install -y git make rsync wget

#
# Install Mamba
#
cd /
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh
bash Miniforge3-Linux-aarch64.sh -b

#
# Clone SlideRule Repository
#
git clone https://github.com/SlideRuleEarth/sliderule.git
cd /sliderule/targets/slideruleearth

#
# Build and Run Self Tests
#
docker pull $CONTAINER_REGISTRY/sliderule-buildenv:$VERSION
make build CONFIG=debug BUILD_CMD="sh -c \"cd /sliderule/targets/slideruleearth && make config-debug && make && make selftest\""

#
# Build Cluster
#
make cluster-docker

#
# SlideRule PyTests
#
cd /sliderule/targets/slideruleearth
/docker-compose up -d ilb ams manager sliderule
cd /sliderule/clients/python
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/sliderule/bin/pip install .
/root/miniforge3/envs/sliderule/bin/pytest --domain localhost --organization None
cd /sliderule/targets/slideruleearth
/docker-compose down ilb ams manager sliderule

#
# AMS PyTests
#
cd /sliderule/applications/ams
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/ams/bin/pytest

#
# Manager PyTests
#
cd /sliderule/applications/manager
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/manager/bin/pytest
