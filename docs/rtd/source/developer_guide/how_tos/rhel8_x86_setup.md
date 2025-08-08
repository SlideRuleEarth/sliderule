# RHEL8 and Fedora Development Environments for GPU Enabled x86 Instances

2024-5-13

## Overview

This write up provides steps in setting up a RHEL8 or Fedora development environment for GPU enabled x86 instances in AWS.  These steps were used when benchmarking machine learning models developed at University of Texas at Austin and Oregon State University for use in ATL24.   

## Steps

### Logging in the first time

RHEL8
```bash
ssh -i .ssh/<mykey>.pem ec2-user@<ip address>
sudo subscription-manager register
sudo dnf upgrade --refresh
```

Fedora39
```bash
ssh -i .ssh/<mykey>.pem fedora@<ip address>
sudo dnf upgrade --refresh
sudo hostnamectl set-hostname --static <new hostname>
```

### Install Large File System for Git
```bash
sudo dnf install wget
wget --content-disposition "https://packagecloud.io/github/git-lfs/packages/el/8/git-lfs-3.5.1-1.el8.x86_64.rpm/download.rpm?distro_version_id=205"
sudo rpm -i git-lfs-3.5.1-1.el8.x86_64.rpm
git lfs install
```

### Configure the development repositories for the package manager:
```bash
sudo subscription-manager config --rhsm.manage_repos=1
sudo subscription-manager repos --enable=codeready-builder-for-rhel-8-x86_64-rpms
sudo dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
sudo dnf install -y epel-release
```

### Install the build requirements:
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install \
    cmake \
    cppcheck \
    opencv-devel \
    python3.9  \
    parallel \
    gmp-devel \
    mlpack-devel \
    mlpack-bin \
    gdal-devel \
    armadillo-devel \
    gcc-toolset-12
```

### Install the NVIDIA drivers:
```bash
dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/rhel8/x86_64/cuda-rhel8.repo
dnf -y install cuda libcudnn8 libcudnn8-devel

distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.repo | sudo tee /etc/yum.repos.d/nvidia-docker.repo
sudo yum install -y nvidia-container-toolkit
sudo systemctl restart docker
```

### Setup the Python environment for the build
```bash
python3.9 -m venv venv
source ./venv/bin/activate
python -m pip install --upgrade pip
pip install torch numpy pandas
```

### Configure Environment for Build

```bash
source ~/venv/bin/activate
source /opt/rh/gcc-toolset-12/enable
CUDACXX=/usr/local/cuda/bin/nvcc
export PATH=/usr/local/cuda/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
```