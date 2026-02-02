# Setting Up Amazon Linux Development Environment

2026-01-28

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

### 7. Clone Repository

```bash
git clone git@github.com:SlideRuleEarth/sliderule.git
```

Install `pip` for pre-commit hooks
```bash
sudo dnf install git
```

Install the pre-commit hooks
```bash
cd sliderule
pip install pre-commit
pre-commit install
pre-commit run --all-files
```

### 8. Installing and Configuring Docker

To install docker
```bash
sudo dnf install docker curl
sudo usermod -aG docker <username>
newgrp docker
```

To start docker
```bash
sudo systemctl start docker
sudo systemctl enable docker
```

Then install Docker Compose plugin
```bash
DOCKER_CONFIG=${DOCKER_CONFIG:-$HOME/.docker}
mkdir -p $DOCKER_CONFIG/cli-plugins
curl -SL https://github.com/docker/compose/releases/download/v2.23.3/docker-compose-linux-aarch64 -o $DOCKER_CONFIG/cli-plugins/docker-compose
chmod +x $DOCKER_CONFIG/cli-plugins/docker-compose
```

### 9. Install Dependencies for Local Build

I you want to build the software on your local machine for local development and debugging, then navigate to the `targets/slideruleearth/docker/sliderule/Dockerfile.buildenv` Docker file and use it as guide to create a development environment for your local build machine. This file is the Docker file that builds and executes the server software that runs in production and includes all the tools necessary to build and debug the code.

If you want to simply build the Docker containers used by SlideRule, then there is no need to setup a local development environment as everything is built inside the containers.

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

#### Single Sign On

Run the following commands the first time to setup your machine:
```bash
aws configure sso --profile <profile> --use-device-code
```

Then to login:
```
mv ~/.hidden_aws ~/.aws || true
unset AWS_PROFILE
aws sso login --profile <profile> --use-device-code
aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
eval $(aws configure export-credentials --profile <profile> --format env)
export AWS_PROFILE=<profile>
```

And to logout (which is very useful for running as IAM role assigned to the instance):
```
mv ~/.aws ~/.hidden_aws || true
unset AWS_PROFILE
```

#### 2-Factor Authentication

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
