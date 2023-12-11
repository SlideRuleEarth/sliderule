# Setting Up Development Environment

2023-12-11

## Overview

These steps setup a development environment for SlideRule.  The target platform is a Graviton3 processor running Amazon Linux 2023 in AWS.

## Steps

### 1. Logging in the first time

```bash
ssh -i .ssh/<mykey>.pem ec2-user@<ip address>
sudo dnf upgrade --refresh
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

### 4. Managing Keys for Remote Login

- create a local key on your laptop or desktop for remote use: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- your new key is located in `.ssh/id_rsa.pub`
- to log into a remote server with this key, while on the remote server copy and paste the key into the file `~/.ssh/authorized_keys`

### 5. Managing Keys for GitHub

- create a key on the server, while logged into your account: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- add the key as an ssh key to your github account

### 6. Install and Configure Git

```bash
sudo dnf install git
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
git clone git@github.com:ICESat2-SlideRule/sliderule.git
git clone git@github.com:ICESat2-SlideRule/sliderule-python.git
```

### 8. Installing and Configuring Docker

```bash
sudo dnf install docker
sudo usermod -aG docker <username>
newgrp docker
```

### 9. Install Dependencies got Local Build

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install \
  cmake \
  readline-devel \
  lua-devel \
  openssl-devel
  libuuid-devel \
  libtiff-devel \
  sqlite-devel \
  curl-devel \
  python-devel \
  meson \
  llvm \
  clang \
  clang-tools-extra \
  cppcheck
```

#### install rapidjson dependency
git clone https://github.com/Tencent/rapidjson.git
cd rapidjson
mkdir build
cd build
cmake ..
make -j8
sudo make install
sudo ldconfig

#### install arrow dependency
git clone https://github.com/apache/arrow.git
cd arrow
mkdir build
cd build
cmake ../cpp -DARROW_PARQUET=ON -DARROW_WITH_ZLIB=ON -DARROW_WITH_SNAPPY=ON
make -j8
sudo make install
sudo ldconfig

#### install proj9 gdal/pdal dependency
git clone https://github.com/OSGeo/PROJ.git
cd PROJ
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
sudo make install
sudo ldconfig

#### install geotiff gdal/pdal dependency
git clone https://github.com/OSGeo/libgeotiff.git
cd libgeotiff
mkdir build
cd build
cmake ../libgeotiff -DCMAKE_BUILD_TYPE=Release
make -j8
sudo make install
sudo ldconfig

#### install geos gdal dependency
git clone https://github.com/libgeos/geos.git
cd geos
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
sudo make install
sudo ldconfig

#### install gdal dependency
git clone https://github.com/OSGeo/gdal.git
cd gdal
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
sudo make install
sudo ldconfig

#### install pistache dependency
git clone https://github.com/pistacheio/pistache.git
cd pistache
meson setup build
sudo meson install -C build
sudo ldconfig


### 10. Install and Configure Miniconda

```bash
wget https://repo.anaconda.com/miniconda/Miniconda3-py39_4.12.0-Linux-aarch64.sh
chmod +x Miniconda3-py39_4.12.0-Linux-aarch64.sh
./Miniconda3-py39_4.12.0-Linux-aarch64.sh
conda config --set changeps1 False
conda config --set auto_activate_base false
```

Once you have miniconda setup, you can navigate to the `sliderule-python` repository and run `conda env create -f environment.yml` to create a `sliderule_env` environment with everything you will need for the SlideRule Python client.

### 11. Install GitHub Command Line Client

```bash
sudo dnf install 'dnf-command(config-manager)'
sudo dnf config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo
sudo dnf install gh
```

### 12. Install and Configure AWS Command Line Client

```bash
sudo dnf install awscli
```

Make sure to setup an initial .aws/credentials file so that it has the sliderule profile access key and secret access key.  The credentials file will look something like:
```
[default]
aws_access_key_id = _
aws_secret_access_key = _
aws_session_token = _

[sliderule]
aws_access_key_id = _
aws_secret_access_key = _
```

To populate the default keys and session token, run:
```bash
aws --profile=sliderule sts get-session-token --serial-number arn:aws:iam::$account_number:mfa/$user_name --token-code=$code
```

To login to the AWS Elastic Container Registry, run:
```bash
aws ecr get-login-password --region $region | docker login --username AWS --password-stdin $account_number.dkr.ecr.$region.amazonaws.com
```

### 13. Install Terraform & Packer

```bash
sudo apt install unzip

wget https://releases.hashicorp.com/terraform/1.3.1/terraform_1.3.1_linux_arm64.zip
unzip terraform_1.3.1_linux_arm64.zip
sudo mv terraform /usr/local/bin/

wget https://releases.hashicorp.com/packer/1.8.3/packer_1.8.3_linux_arm64.zip
unzip packer_1.8.3_linux_arm64.zip
sudo mv packer /usr/local/bin/
```
