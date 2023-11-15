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

### 6. Install and Configure Git

```bash
sudo apt install git
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
sudo apt-get install ca-certificates curl gnupg lsb-release
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo usermod -aG docker <username>
newgrp docker
```

### 9. Install Dependencies got Local Build

The most reliable way to install all of the dependencies needed to build sliderule is to follow the steps outlined in the [Dockerfile](https://github.com/ICESat2-SlideRule/sliderule/blob/main/targets/slideruleearth-aws/docker/sliderule/Dockerfile.buildenv) for building the development environment.  Some translation of the steps from the Dockerfile format is needed, but all of the dependencies and the configuration options needed for those dependencies are explicitly called out in that file.

Alternatively, the `sliderule-buildenv` Docker image can be built and used as your development environment.  To do so, run the following commands from the root of the sliderule repository.
```bash
cd targets/slideruleearth-aws
make sliderule-buildenv-docker
make run-buildenv
```
Then, inside the container, you'll find the sliderule repository at `/host/sliderule`.  Change directory there and you will then be able to build the sliderule code however you'd like use the provided makefiles under the root and `targets` directory.

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
type -p curl >/dev/null || sudo apt install curl -y
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
&& sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
&& echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
&& sudo apt update \
&& sudo apt install gh -y
```

### 12. Install and Configure AWS Command Line Client

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
