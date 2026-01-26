#!/bin/bash
exec > /testrunner.log 2>&1
export HOME=/root # needed for pytests
BRANCH=$1

#
# System Dependencies
#
dnf update -y
dnf install -y git make rsync wget docker

#
# Configure Docker
#
systemctl enable docker
systemctl start docker
usermod -aG docker ec2-user

#
# Install Docker Compose V2
#
curl -L "https://github.com/docker/compose/releases/download/v2.24.5/docker-compose-linux-aarch64" -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose
mkdir -p /usr/local/lib/docker/cli-plugins
ln -s /usr/local/bin/docker-compose /usr/local/lib/docker/cli-plugins/docker-compose

#
# ILB
#
mkdir -p /etc/ssl/private
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/$DOMAIN.pem /etc/ssl/private/$DOMAIN.pem

#
# AMS
#
mkdir -p /data/ATL13
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/atl13.json /data/ATL13/atl13.json
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/atl13.db /data/ATL13/atl13.db
mkdir -p /data/ATL24
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/atl24r2.db /data/ATL24/atl24r2.db
mkdir -p /data/3DEP
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/3dep.db /data/3DEP/3dep.db

#
# Plugins
#
mkdir -p /plugins
aws s3 cp s3://$PROJECT_BUCKET/plugins/ /plugins/ --recursive

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
git checkout $BRANCH

#
# Build and Run Self Tests
#
docker pull $CONTAINER_REGISTRY/sliderule-buildenv:latest
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
/opt/miniforge3/bin/mamba env create -f environment.yml -y
/opt/miniforge3/envs/sliderule/bin/pip install .
/opt/miniforge3/envs/sliderule/bin/pytest --domain localhost --organization None
cd /sliderule/targets/slideruleearth
/docker-compose down ilb ams manager sliderule

#
# AMS PyTests
#
cd /sliderule/applications/ams
/opt/miniforge3/bin/mamba env create -f environment.yml -y
/opt/miniforge3/envs/ams/bin/pytest

#
# Manager PyTests
#
cd /sliderule/applications/manager
/opt/miniforge3/bin/mamba env create -f environment.yml -y
/opt/miniforge3/envs/manager/bin/pytest
