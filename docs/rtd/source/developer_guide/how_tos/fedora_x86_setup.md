# Setting Up Fedora39 Development Environment

2024-5-21

## Overview

These steps setup a development environment for SlideRule on a GPU enabled x86 based processor running Fedora39 in AWS.

## Steps

### 1. Logging in the first time

```bash
ssh -i .ssh/<mykey>.pem fedora@<ip address>
sudo dnf upgrade --refresh
```

```bash
sudo hostnamectl set-hostname --static <new hostname>
```

### 2. Creating Users

```bash
sudo useradd -m -s /usr/bin/bash <username>
sudo passwd <username>
sudo usermod -aG wheel <username>
su - <username>
```

### 3. Setup Bash

Replace the appropriate section in the .bashrc file with the contents below
```bash
parse_git_branch() {
     git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/ (\1)/'
}
PS1="\n\[\033[01;32m\]\u@\h\[\033[00m\]\[\033[33m\] [\$CONDA_DEFAULT_ENV]\$(parse_git_branch):\[\033[01;34m\]\w\[\033[00m\]\n\$ "
```

Setup core dumps by adding the following to the end of the .bashrc file
```bash
ulimit -c unlimited
```

### 4. Managing Keys for GitHub

- create a key on the server, while logged into your account: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- add the key as an ssh key to your github account

### 5. Managing Keys for Remote Login

- create a local key on your laptop or desktop for remote use: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- your new key is located in `.ssh/id_rsa.pub`
- to log into a remote server with this key, while on the remote server copy and paste the key into the file `~/.ssh/authorized_keys`
- make sure the permissions on the `.ssh/authorized_keys` file is `644`

### 6. Install and Configure Git

```bash
sudo dnf install git gh git-lfs
git lfs install
```

Create the local file `~/.gitconfig` in the user's home directory with the following contents:
```yml
[user]
	name = <name>
	email = <email>

[push]
    default = simple

[diff]
    tool = vimdiff

[merge]
    tool = vimdiff

[difftool]
    prompt = false

[mergetool]
    keepBackup = false

[difftool "vimdiff"]
    cmd = vimdiff "$LOCAL" "$REMOTE"

[alias]
    dd = difftool --dir-diff

[credential "https://github.com"]
	helper =
	helper = !/usr/bin/gh auth git-credential

[credential "https://gist.github.com"]
	helper =
	helper = !/usr/bin/gh auth git-credential
```

### 7. Clone Projects

```bash
mkdir meta
cd meta
git clone https://github.com/jeffsp/coastnet
git clone git@github.com:SlideRuleEarth/sliderule.git
git clone git@github.com:ICESat2-SlideRule/openoceans-dev.git
```

### 8. Install Dependencies for Local Build

First configure the development repositories for the package manager:
```bash
sudo subscription-manager config --rhsm.manage_repos=1
sudo subscription-manager repos --enable=codeready-builder-for-rhel-8-x86_64-rpms
sudo dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
sudo dnf install -y epel-release
```

Install the build requirements:
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

Install the NVIDIA drivers:
```bash
dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/rhel8/x86_64/cuda-rhel8.repo
dnf -y install cuda libcudnn8 libcudnn8-devel
```

distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.repo | sudo tee /etc/yum.repos.d/nvidia-docker.repo
sudo yum install -y nvidia-container-toolkit
sudo systemctl restart docker

Setup the Python environment for the build
```bash
python3.9 -m venv venv
source ./venv/bin/activate
python -m pip install --upgrade pip
pip install torch numpy pandas
```

### 9. Configure Environment for Build

```bash
source ~/venv/bin/activate
source /opt/rh/gcc-toolset-12/enable
CUDACXX=/usr/local/cuda/bin/nvcc
```

export PATH=/usr/local/cuda/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}