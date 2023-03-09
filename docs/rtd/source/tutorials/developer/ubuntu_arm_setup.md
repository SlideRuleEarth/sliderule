# Setting Up Development Environment

2022-10-07

## Overview

These steps setup a development environment for SlideRule.  The target platform is a Graviton3 processor running Ubuntu 20.04 in AWS.

## Steps

### 1. Logging in the first time

```bash
ssh -i .ssh/<mykey>.pem ubuntu@<ip address>
sudo apt update
sudo apt upgrade
```

### 2. Creating Users

```bash
sudo useradd -m -s /usr/bin/bash <username>
sudo passwd <username>
sudo usermod -aG sudo <username>
su - <username>
```

### 3. Setup Bash

Replace the appropriate section in the .bashrc file with the contents below
```bash
force_color_prompt=yes

parse_git_branch() {
     git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/ (\1)/'
}

if [ "$color_prompt" = yes ]; then
    PS1="\n${debian_chroot:+($debian_chroot)}\[\033[01;32m\]\u@\h\[\033[00m\]\[\033[33m\] [\$CONDA_DEFAULT_ENV]\$(parse_git_branch):\[\033[01;34m\]\w\[\033[00m\]\n\$ "
else
    PS1='${debian_chroot:+($debian_chroot)}\u@\h:\w\$ '
fi

unset color_prompt force_color_prompt
```

Setup core dumps by adding the following to the end of the .bashrc file
```bash
ulimit -c unlimited
```

In order to get core dumps, make sure the apport service is enabled and running, and then look for them in `/var/lib/apport/coredump/`
```bash
sudo systemctl enable apport.service
```

### 4. Managing Keys for Remote Login

- create a local key on your laptop or desktop for remote use: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- your new key is located in `.ssh/id_rsa.pub`
- to log into a remote server with this key, while on the remote server copy and paste the key into the file `~/.ssh/authorized_keys`

### 5. Managing Keys for GitHub

- create a key on the server, while logged into your account: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
- add the key as an ssh key to your github account

### 6. Update and Install OS Packages

The following OS dependencies are needed to build the SlideRule server.
```bash
sudo apt install build-essential libreadline-dev liblua5.3-dev
sudo apt install git cmake
sudo apt install curl libcurl4-openssl-dev
sudo apt install zlib1g-dev
sudo apt install libgdal-dev
```

Note: starting with release v2.1.0, SlideRule requires a >=3.6.0 version of GDAL.  This can require building GDAL from source.  See the SlideRule cluster Dockerfile for details on how that could be accomplished.

### 7. Install RapidJson

RapidJson is used by SlideRule for parsing JSON in the C++ server-side code.
```bash
git clone https://github.com/Tencent/rapidjson.git
cd rapidjson
mkdir build
cd build
cmake ..
make
sudo make install
```

### 8. Install Apache Arrow

Apache Arrow is used by SlideRule for its GeoParquet support.
```bash
git clone https://github.com/apache/arrow.git
cd arrow
mkdir build
cd build
cmake .. -DARROW_PARQUET=ON -DARROW_WITH_ZLIB=ON
make -j8
sudo make install
```

### 9. Install Pistache

SlideRule supports running either a native HTTP server, or the Pistache HTTP server which requires the Pistache library to be installed.
```bash
$ sudo add-apt-repository ppa:pistache+team/unstable
$ sudo apt update
$ sudo apt install libpistache-dev
```

### 10. Configure Git

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

### 11. Clone Projects

```bash
mkdir meta
cd meta
git clone git@github.com:ICESat2-SlideRule/sliderule.git
git clone git@github.com:ICESat2-SlideRule/sliderule-python.git
git clone git@github.com:ICESat2-SlideRule/sliderule-docs.git
git clone git@github.com:ICESat2-SlideRule/sliderule-build-and-deploy.git
```

### 12. Installing and Configuring Docker

```bash
sudo apt-get install ca-certificates curl gnupg lsb-release
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo usermod -aG docker <username>
newgrp docker
```

### 13. Installing Docker-Compose

```bash
wget https://github.com/docker/compose/releases/download/v2.11.2/docker-compose-linux-aarch64
sudo mv docker-compose-linux-aarch64 /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
```

### 14. Installing NGINX

Install NGINX which is used for running the containers locally.
```bash
sudo apt install nginx
```
Copy over the nginx configuration and restart nginx
```bash
sudo cp nginx.service /etc/nginx/sites-available/sliderule
sudo ln -s /etc/nginx/sites-available/sliderule /etc/nginx/sites-enabled/sliderule
sudo unlink /etc/nginx/sites-enabled/default
```

Create the self-signed certs
```bash
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout /etc/ssl/private/nginx-selfsigned.key -out /etc/ssl/certs/nginx-selfsigned.crt
sudo systemctl restart nginx
```

### 15. Install and Configure Miniconda

```bash
wget https://repo.anaconda.com/miniconda/Miniconda3-py39_4.12.0-Linux-aarch64.sh
chmod +x Miniconda3-py39_4.12.0-Linux-aarch64.sh
./Miniconda3-py39_4.12.0-Linux-aarch64.sh
conda config --set changeps1 False
conda config --set auto_activate_base false
```

### 16. Install GitHub Command Line Client

```bash
type -p curl >/dev/null || sudo apt install curl -y
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
&& sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
&& echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
&& sudo apt update \
&& sudo apt install gh -y
```

### 17. Install and Configure AWS Command Line Client

```bash
sudo apt install awscli
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

### 18. Install Terraform & Packer

```bash
sudo apt install unzip

wget https://releases.hashicorp.com/terraform/1.3.1/terraform_1.3.1_linux_arm64.zip
unzip terraform_1.3.1_linux_arm64.zip
sudo mv terraform /usr/local/bin/

wget https://releases.hashicorp.com/packer/1.8.3/packer_1.8.3_linux_arm64.zip
unzip packer_1.8.3_linux_arm64.zip
sudo mv packer /usr/local/bin/
```
