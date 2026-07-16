#!/bin/bash
export HOME=/root
export USER=root
export AWS_DEFAULT_REGION=$AWS_REGION

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
aws ecr get-login-password --region $AWS_REGION | docker login --username AWS --password-stdin $CONTAINER_REGISTRY

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
# Data
#
mkdir -p /data/
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/ams/ /data/ --recursive
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/server/ /data/ --recursive

#
# Install Mamba
#
cd /
wget -q https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh
bash Miniforge3-Linux-aarch64.sh -b

#
# Setup and Clone SlideRule Repository
#
docker pull $CONTAINER_REGISTRY/sliderule-buildenv:latest
git clone https://github.com/SlideRuleEarth/sliderule.git
cd /sliderule
git checkout $BRANCH

#
# Configure Logging
#
exec > /testrunner.log 2>&1

#
# Build and Run Self Tests
#
cd /sliderule/targets/slideruleearth
make build CONFIG=debug BUILD_CMD="sh -c \"cd /sliderule/targets/slideruleearth && make config-debug && make && make selftest\""

#
# Build Cluster
#
make cluster-docker

#
# SlideRule PyTests
#
cd /sliderule/targets/slideruleearth
docker compose up -d ilb ams sliderule
cd /sliderule/clients/python
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/sliderule/bin/pip install .
/root/miniforge3/envs/sliderule/bin/pytest --domain localhost --organization None
cd /sliderule/targets/slideruleearth
docker compose down ilb ams sliderule

#
# Provisioner PyTests
#
cd /sliderule/apps/provisioner
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/provisioner/bin/pytest

#
# AMS PyTests
#
cd /sliderule/apps/ams
/root/miniforge3/bin/mamba env create -f environment.yml -y
/root/miniforge3/envs/ams/bin/pytest

#
# Record of Run
#
aws s3 cp /testrunner.log s3://$PROJECT_BUCKET/testrunner/$BRANCH-$DEPLOY_DATE-testrunner.log
cd /sliderule/apps/provisioner
/root/miniforge3/envs/provisioner/bin/python utils/testrunner_report.py --testfile $BRANCH-$DEPLOY_DATE-testrunner.log --testdir s3://$PROJECT_BUCKET/testrunner --branch $BRANCH --output /tmp/$BRANCH-summary.json
aws s3 cp /tmp/$BRANCH-summary.json s3://$PROJECT_BUCKET/testrunner/$BRANCH-summary.json
