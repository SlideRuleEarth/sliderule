# In order to complete the release process, the following must be installed and configured in your environment:
#     * GitHub command line client (gh)
#           See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#           The user must authenticate to GitHub via `gh auth login`
#     * AWS command line client (awscli)
#           See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#           The user must have up-to-date aws credentials
#     * Docker
#           The user must be logged into the AWS Elastic Container Registry (ECR)
#     * Node.js
#			The javascript npm package for SlideRule is updated via a node.js script on release
#     * conda-lock
#           The Python base image used for the container runtime environment uses conda-lock to create the conda dependency file
#
# To release a version of SlideRule:
#     1. Update .aws/credentials file with a temporary session token; {profile} references your long term aws credentials, {username} is your aws account, {code} is your MFA token
#        $ aws --profile={profile} sts get-session-token --serial-number arn:aws:iam::742127912612:mfa/{username} --token-code={code}
#     2. Login to AWS Elastic Container Registry
#        $ aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
#     3. Login to GitHub
#        $ gh auth login
#     4. Select version of `npm`
#        $ nvm use 20
#     5. Login to NPMJS
#        $ cd sliderule/clients/nodejs && make login
#     6. Execute the makefile target to release the code; X.Y.Z is the version
#        $ make release VERSION=vX.Y.Z
#
# To update the build environment for GitHub actions
#     1. Login to GitHub Container Registry; {my_github_key} is a file storing an access key created for your account
#        $ cat {my_github_key} | docker login ghcr.io -u jpswinski --password-stdin
#     2. Build the docker build environment on x86 machine (x86 is IMPORTANT to match GitHub actions environment)
#        $ make buildenv-docker
#     3. Push to GitHub container registry
#        $ docker push ghcr.io/slideruleearth/sliderule-buildenv:latest
#

ROOT = $(shell realpath $(shell pwd)/../..)
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
SLIDERULE_STAGE_DIR = $(STAGE)/sliderule
BUILDENV_STAGE_DIR = $(STAGE)/buildenv
ILB_STAGE_DIR = $(STAGE)/ilb
PROXY_STAGE_DIR = $(STAGE)/proxy
MONITOR_STAGE_DIR = $(STAGE)/monitor
MONITOR_AGENT_STAGE_DIR = $(STAGE)/monitor-agent
MANAGER_STAGE_DIR = $(STAGE)/manager
AMS_STAGE_DIR = $(STAGE)/ams
CERTBOT_STAGE_DIR = $(STAGE)/certbot
TESTBOT_STAGE_DIR = $(STAGE)/testbot
PROVISIONER_STAGE_DIR = $(STAGE)/provisioner
WEBSITE_SOURCE_DIR = $(ROOT)/docs
WEBSITE_STAGE_DIR = $(STAGE)/website
WEBSITE_EXPORT_DIR = $(WEBSITE_STAGE_DIR)/dist
WEBSITE_ASSET_DIR = /data/web
PYTHON_CLIENT_DIR = $(ROOT)/clients/python
PYTHON_EXAMPLES_DIR = $(PYTHON_CLIENT_DIR)/examples
NODEJS_CLIENT_DIR = $(ROOT)/clients/nodejs
CF_STAGE_DIR = $(STAGE)/cf

INSTALLDIR ?= $(SLIDERULE_STAGE_DIR)

PROJECT_BUCKET = sliderule
PROJECT_FOLDER = cf
PROJECT_PUBLIC_BUCKET = sliderule-public
PROJECT_WEBSITE_BUCKET = sliderule-website
AWS_ACCOUNT = 742127912612
AWS_REGION = us-west-2
ECR = $(AWS_ACCOUNT).dkr.ecr.$(AWS_REGION).amazonaws.com

MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER ?= $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
ARCH ?= $(shell arch)
TIMESTAMP ?= $(shell date +%Y%m%d%H%M%S)
CONFIG ?= release

DOMAIN ?= slideruleearth.io
ORGANIZATION ?= developers
VERSION ?= latest

DOCKEROPTS ?= # --progress=plain --no-cache
BUILDX ?= # buildx
DOCKER_PLATFORM ?= # --platform linux/arm64 | linux/amd64
USERCFG ?= # e.g. -DSKIP_STATIC_ANALYSIS=1
RUNNER ?= # e.g. valgrind
BUILD_CMD ?= sh -c "cd $(ROOT)/targets/slideruleearth && make config-$(CONFIG) && make"

CLANG_VER ?= ""
CLANG_CFG = export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER)

COMMON_CFG := -DINSTALLDIR=$(INSTALLDIR)
COMMON_CFG += -DUSE_HDF_PACKAGE=ON
COMMON_CFG += $(USERCFG)

DEBUG_CFG := -DCMAKE_BUILD_TYPE=Debug
DEBUG_CFG += -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
DEBUG_CFG += -DENABLE_ADDRESS_SANITIZER=ON
DEBUG_CFG += -DSKIP_STATIC_ANALYSIS=OFF      # ON to disable static analysis for faster debug builds, set to OFF before committing
DEBUG_CFG += $(COMMON_CFG)

RELEASE_CFG := -DCMAKE_BUILD_TYPE=Release
RELEASE_CFG += $(COMMON_CFG)

########################
# Local Build Targets
########################

all: sliderule

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)

config-debug: prep ## configure the server for running locally with debug symbols, optimizations off, static analysis, and address sanitizer
	cd $(SLIDERULE_BUILD_DIR) && $(CLANG_CFG) && cmake $(DEBUG_CFG) -DMAX_FREE_STACK_SIZE=1 -DENABLE_TRACING=ON $(ROOT)

config-release: prep ## configure server to run a release version locally
	cd $(SLIDERULE_BUILD_DIR) && cmake $(RELEASE_CFG) -DMAX_FREE_STACK_SIZE=1 $(ROOT)

sliderule: ## build the server using the local configuration
	make -j8 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install

run: ## run the server locally
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) IS_PUBLIC=False ASAN_OPTIONS="detect_leaks=1" LSAN_OPTIONS="suppressions=$(ROOT)/targets/slideruleearth/asan_leaks.supp" $(RUNNER) $(INSTALLDIR)/bin/sliderule $(ROOT)/targets/slideruleearth/server.lua config.json

build: ## build the code using the build environment docker container
	docker run \
		-it \
		--network host \
		--user $(shell id -u):$(shell id -g) \
		-v $(shell realpath ~/.ssh):/root/.ssh \
		-v $(ROOT):$(ROOT) \
		-v /data:/data \
		-e MYIP=$(MYIP) \
		-e ROOT=$(ROOT) \
		-e GITNAME="$(shell git config user.name)" \
		-e GITEMAIL="$(shell git config user.email)" \
		--rm \
		--name buildenv \
		$(ECR)/sliderule-buildenv:$(VERSION) \
		$(BUILD_CMD)

########################
# Docker Build Targets
########################

buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cp docker/sliderule/.bashrc $(BUILDENV_STAGE_DIR)
	cd $(BUILDENV_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/sliderule-buildenv:$(VERSION) $(DOCKER_PLATFORM) .
	docker run -it -v ./docker/sliderule:/host --rm --name buildenv $(ECR)/sliderule-buildenv:$(VERSION) /bin/bash /host/libdep.sh /host/libdep-$(ARCH).lock

buildenv-docker-push: ## push build environment docker image to registry
	docker push $(ECR)/sliderule-buildenv:$(VERSION)

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SLIDERULE_STAGE_DIR)
	mkdir -p $(SLIDERULE_STAGE_DIR)
	rsync -a $(ROOT) $(SLIDERULE_STAGE_DIR) --exclude build --exclude stage
	cp docker/sliderule/Dockerfile.runtime $(SLIDERULE_STAGE_DIR)/Dockerfile
	cp docker/sliderule/docker-entrypoint.sh $(SLIDERULE_STAGE_DIR)/
	cp docker/sliderule/config.json $(SLIDERULE_STAGE_DIR)
	cd $(SLIDERULE_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) --build-arg repo=$(ECR) -t $(ECR)/sliderule:$(VERSION) $(DOCKER_PLATFORM) .

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/ilb/* $(ILB_STAGE_DIR)
	cp $(ROOT)/applications/ilb/orchestrator.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/packages/core/extensions/json.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/packages/core/extensions/prettyprint.lua $(ILB_STAGE_DIR)
	cd $(ILB_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/ilb:$(VERSION) $(DOCKER_PLATFORM) .

proxy-docker: # make the reverse proxy docker image; needs VERSION
	-rm -Rf $(PROXY_STAGE_DIR)
	mkdir -p $(PROXY_STAGE_DIR)
	cp docker/proxy/* $(PROXY_STAGE_DIR)
	cd $(PROXY_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/proxy:$(VERSION) $(DOCKER_PLATFORM) .

monitor-docker: ## build monitor docker image; needs VERSION
	-rm -Rf $(MONITOR_STAGE_DIR)
	mkdir -p $(MONITOR_STAGE_DIR)
	cp -R docker/monitor/rootfs/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	cd $(MONITOR_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/monitor:$(VERSION) $(DOCKER_PLATFORM) .

monitor-agent-docker: ## build monitor agent docker image; needs VERSION
	-rm -Rf $(MONITOR_AGENT_STAGE_DIR)
	mkdir -p $(MONITOR_AGENT_STAGE_DIR)
	cp -R docker/monitor-agent/rootfs/* $(MONITOR_AGENT_STAGE_DIR)
	cp docker/monitor-agent/Dockerfile.$(ARCH) $(MONITOR_AGENT_STAGE_DIR)/Dockerfile
	cd $(MONITOR_AGENT_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/monitor-agent:$(VERSION) $(DOCKER_PLATFORM) .

ams-lock: ## update dependencies of ams
	cd docker/ams && conda-lock -p linux-$(ARCH) -f $(ROOT)/applications/ams/environment.yml
	cd docker/ams && conda-lock render -p linux-$(ARCH)

ams-docker: ## build ams docker image; uses VERSION
	-rm -Rf $(AMS_STAGE_DIR)
	mkdir -p $(AMS_STAGE_DIR)
	cp docker/ams/Dockerfile.$(ARCH) $(AMS_STAGE_DIR)/Dockerfile
	cp docker/ams/conda-* $(AMS_STAGE_DIR)
	cp docker/ams/docker-entrypoint.sh $(AMS_STAGE_DIR)
	cp $(ROOT)/applications/ams/pyproject.toml $(AMS_STAGE_DIR)
	cp $(ROOT)/version.txt $(AMS_STAGE_DIR)
	chmod +x $(AMS_STAGE_DIR)/docker-entrypoint.sh
	mkdir $(AMS_STAGE_DIR)/ams
	cp $(ROOT)/applications/ams/ams/*.py $(AMS_STAGE_DIR)/ams
	cd $(AMS_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/ams:$(VERSION) $(DOCKER_PLATFORM) .

ams-run: ## run ams locally; uses VERSION
	docker run -it -p 8060:8060 --rm --name ams $(ECR)/ams:$(VERSION) /bin/bash /docker-entrypoint.sh

manager-lock: ## update dependencies of manager
	cd docker/manager && conda-lock -p linux-$(ARCH) -f $(ROOT)/applications/manager/environment.yml
	cd docker/manager && conda-lock render -p linux-$(ARCH)

manager-docker: ## build manager docker image; uses VERSION
	-rm -Rf $(MANAGER_STAGE_DIR)
	mkdir -p $(MANAGER_STAGE_DIR)
	cp docker/manager/Dockerfile.$(ARCH) $(MANAGER_STAGE_DIR)/Dockerfile
	cp docker/manager/conda-* $(MANAGER_STAGE_DIR)
	cp docker/manager/docker-entrypoint.sh $(MANAGER_STAGE_DIR)
	cp $(ROOT)/applications/manager/pyproject.toml $(MANAGER_STAGE_DIR)
	cp $(ROOT)/version.txt $(MANAGER_STAGE_DIR)
	chmod +x $(MANAGER_STAGE_DIR)/docker-entrypoint.sh
	mkdir $(MANAGER_STAGE_DIR)/manager
	cp $(ROOT)/applications/manager/manager/*.py $(MANAGER_STAGE_DIR)/manager
	cp $(ROOT)/applications/manager/manager/*.sql $(MANAGER_STAGE_DIR)/manager
	cd $(MANAGER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/manager:$(VERSION) $(DOCKER_PLATFORM) .

manager-run: ## run manager locally; uses VERSION
	docker run -it -p 8040:8040 --rm --name manager $(ECR)/manager:$(VERSION) /bin/bash /docker-entrypoint.sh

cluster-docker: ## build all cluster docker images
	make sliderule-docker
	make ilb-docker
	make proxy-docker
	make monitor-docker
	make monitor-agent-docker
	make manager-docker
	make ams-docker

cluster-docker-push: cluster-cf-update ## push all cluster images to container registries (and docker compose files to S3)
	docker push $(ECR)/sliderule:$(VERSION)
	docker push $(ECR)/ilb:$(VERSION)
	docker push $(ECR)/proxy:$(VERSION)
	docker push $(ECR)/monitor:$(VERSION)
	docker push $(ECR)/monitor-agent:$(VERSION)
	docker push $(ECR)/manager:$(VERSION)
	docker push $(ECR)/ams:$(VERSION)

cluster-cf-update:
	aws s3 cp cloudformation/ s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(VERSION)/ --recursive --exclude "*" --include "docker-compose-*.yml"

cluster-cf-init:
	-rm -Rf $(CF_STAGE_DIR)
	mkdir -p $(CF_STAGE_DIR)
	wget --directory-prefix=$(CF_STAGE_DIR) https://github.com/docker/compose/releases/download/v2.29.1/docker-compose-linux-aarch64
	aws s3 cp $(CF_STAGE_DIR)/docker-compose-linux-aarch64 s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/

########################
# Deployment Targets
########################
# For public clusters the CLUSTER variable is set to sliderule-blue|green while the ORGANIZATION remains as sliderule.
# This is so the DNS can be changed to a cluster that is already up and running.

PUBLIC_IP ?= 127.0.0.1
IS_PUBLIC ?= False
NODE_CAPACITY ?= 1
CLUSTER ?= $(ORGANIZATION)

deployment-create: ## create a cluster in AWS; e.g. make deployment-create ORGANIZATION=sliderule CLUSTER=sliderule-<color> VERSION=<version> NODE_CAPACITY=7 IS_PUBLIC=True
	aws cloudformation create-stack \
		--stack-name 	$(CLUSTER)-cluster \
		--template-body file://cloudformation/cluster.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters 	ParameterKey=Version,ParameterValue=$(VERSION) \
						ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=IsPublic,ParameterValue=$(IS_PUBLIC) \
						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=Cluster,ParameterValue=$(CLUSTER) \
						ParameterKey=Organization,ParameterValue=$(ORGANIZATION) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=ProjectPublicBucket,ParameterValue=$(PROJECT_PUBLIC_BUCKET) \
						ParameterKey=ContainerRegistry,ParameterValue=$(ECR) \
						ParameterKey=NodeCapacity,ParameterValue=$(NODE_CAPACITY)
	aws cloudformation wait stack-create-complete --stack-name $(CLUSTER)-cluster

deployment-describe: ## status the creation/deletion of the cluster
	aws cloudformation describe-stack-events --stack-name $(CLUSTER)-cluster

deployment-delete: ## deletes the cluster
	aws cloudformation delete-stack --stack-name $(CLUSTER)-cluster
	aws cloudformation wait stack-delete-complete --stack-name $(CLUSTER)-cluster

deployment-go-live: ## switch dns entry of public cluster; e.g. make deployment-go-live ORGANIZATION=sliderule PUBLIC_IP=<ip_address>
	aws cloudformation deploy \
		--stack-name 	dns \
		--template-file cloudformation/dns.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameter-overrides \
		 	Domain=$(DOMAIN) \
			Organization=$(ORGANIZATION) \
			PublicIp=$(PUBLIC_IP)

deployment-take-offline: ## destroy dns entry of public cluster
	aws cloudformation delete-stack --stack-name dns

CLUSTER_PARMS = '{\
	"Version": "$(VERSION)",\
	"IsPublic": "$(IS_PUBLIC)",\
	"Domain": "$(DOMAIN)",\
	"Organization": "$(ORGANIZATION)",\
	"Cluster": "$(CLUSTER)",\
	"ProjectBucket": "$(PROJECT_BUCKET)",\
	"ProjectFolder": "$(PROJECT_FOLDER)",\
	"ProjectPublicBucket": "$(PROJECT_PUBLIC_BUCKET)",\
	"ContainerRegistry": "$(ECR)",\
	"NodeCapacity": "$(NODE_CAPACITY)",\
	"Region": "$(AWS_REGION)"\
}'

deployment-invoke:
	echo $(CLUSTER_PARMS) > /tmp/request.json
#	aws lambda invoke --function-name Provisioner --payload fileb:///tmp/request.json /tmp/response.json
#	cat /tmp/response.json | jq

########################
# Testbot Targets
########################

testrunner-update: ## updates the testrunner script
	aws s3 cp $(ROOT)/applications/testbot/testrunner.sh s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(VERSION)/testrunner.sh

#testrunner-create: testbot-build testrunner-update ## create a test runner in AWS
# 	aws s3 cp $(TESTBOT_STAGE_DIR)/testbot.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/testbot.zip
# 	aws cloudformation create-stack \
# 		--stack-name 	testrunner \
# 		--template-body file://cloudformation/testrunner.yml \
# 		--capabilities 	CAPABILITY_NAMED_IAM \
# 		--region 		$(AWS_REGION) \
# 		--parameters 	ParameterKey=Version,ParameterValue=$(VERSION) \
# 						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
# 						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
# 						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
# 						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/testbot.zip \
# 						ParameterKey=ContainerRegistry,ParameterValue=$(ECR) \
# 						ParameterKey=DeployDate,ParameterValue=$(TIMESTAMP)
# 	aws cloudformation wait stack-create-complete --stack-name testrunner

testrunner-describe: ## status the creation/deletion of the test runner
	aws cloudformation describe-stack-events --stack-name testrunner

testrunner-delete: ## deletes the test runner and any lingering event bridge rules
	aws cloudformation delete-stack --stack-name testrunner
	aws cloudformation wait stack-delete-complete --stack-name testrunner
	- aws events remove-targets --rule testrunner-auto-delete --ids 1
	- aws events delete-rule --name testrunner-auto-delete

testbot-build: ## create the testrunner lambda zipfile
	-rm -Rf $(TESTBOT_STAGE_DIR)
	mkdir -p $(TESTBOT_STAGE_DIR)
	docker run -it --rm \
		-v $(TESTBOT_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.11 \
		-c "pip install --target /host/package urllib3"
	cp $(ROOT)/applications/testbot/main.py $(TESTBOT_STAGE_DIR)/package/
	cp cloudformation/testrunner.yml $(TESTBOT_STAGE_DIR)/package/
	cd $(TESTBOT_STAGE_DIR)/package/ && zip -r -q $(TESTBOT_STAGE_DIR)/testbot.zip . -x "/*__pycache__/*"

testbot-create: testbot-build testrunner-update ## creates the lambda testbot function
	aws s3 cp $(TESTBOT_STAGE_DIR)/testbot.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/testbot.zip
	aws cloudformation create-stack \
		--stack-name 	testbot \
		--template-body file://cloudformation/testbot.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/testbot.zip
	aws cloudformation wait stack-create-complete --stack-name testbot

testbot-describe: ## status the creation/deletion of the testbot function
	aws cloudformation describe-stack-events --stack-name testbot

testbot-delete: ## deletes the lambda testbot function
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/testbot.zip
	aws cloudformation delete-stack --stack-name testbot
	aws cloudformation wait stack-delete-complete --stack-name testbot

testbot-run: ## invokes the testbot lambda function and displays the response
	echo '{"Version":"$(VERSION)","Domain":"$(DOMAIN)","ProjectBucket":"$(PROJECT_BUCKET)","ProjectFolder":"$(PROJECT_FOLDER)","LambdaZipFile":"$(PROJECT_FOLDER)/testbot.zip","ContainerRegistry":"$(ECR)","DeployDate":"$(TIMESTAMP)","Region":"$(AWS_REGION)"}' > /tmp/payload.json
	aws lambda invoke --function-name testbot-run --payload fileb:///tmp/payload.json /tmp/response.json
	cat /tmp/response.json | jq

########################
# Website Targets
########################

DISTRIBUTION_ID = $(shell aws cloudfront list-distributions \
	--query "DistributionList.Items[?Aliases.Items[0]=='$(DOMAIN)'].Id" \
	--output text)

ZONE_ID=$(shell aws route53 list-hosted-zones-by-name \
	--dns-name slideruleearth.io \
	--query "HostedZones[0].Id" \
	--output text | sed 's|/hostedzone/||')

WEBSITE_CONDA_ENV ?= sliderule_env # clients/python/examples/environment.yml
WEBSITE_CONDA_ACTIVATE = source "$(shell conda info --base)/etc/profile.d/conda.sh" && conda activate $(WEBSITE_CONDA_ENV)

CFFUNC_ID = $(shell aws cloudformation describe-stacks \
	--stack-name cffunc \
	--region us-east-1 \
  	--query "Stacks[0].Outputs[?OutputKey=='FunctionARN'].OutputValue" \
	--output text)

CFFUNC_ETAG = $(shell aws cloudfront describe-function \
	--region us-east-1 \
	--name cffunc-uri-rewrite \
	--query 'ETag' \
	--output text)

website-render-notebooks: ## execute the notebooks used in the documentation
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/boulder_watershed.ipynb --output $(WEBSITE_ASSET_DIR)/boulder_watershed.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/grandmesa.ipynb --output $(WEBSITE_ASSET_DIR)/grandmesa.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/phoreal.ipynb --output $(WEBSITE_ASSET_DIR)/phoreal.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/arcticdem_mosaic.ipynb --output $(WEBSITE_ASSET_DIR)/arcticdem_mosaic.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/grandmesa_atl03_classification.ipynb --output $(WEBSITE_ASSET_DIR)/grandmesa_atl03_classification.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/atl13_access.ipynb --output $(WEBSITE_ASSET_DIR)/atl13_access.ipynb --debug
	$(WEBSITE_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/atl24_access.ipynb --output $(WEBSITE_ASSET_DIR)/atl24_access.ipynb --debug

website-docker: ## make the website docker image; needs VERSION
	rm -Rf $(WEBSITE_STAGE_DIR)
	mkdir -p $(WEBSITE_STAGE_DIR)
	cp -R $(WEBSITE_SOURCE_DIR)/rtd $(WEBSITE_STAGE_DIR)
	cp -R $(WEBSITE_SOURCE_DIR)/jekyll $(WEBSITE_STAGE_DIR)
	rsync -a $(ROOT) $(WEBSITE_STAGE_DIR) --exclude build --exclude stage
	cp $(WEBSITE_ASSET_DIR)/* $(WEBSITE_STAGE_DIR)/rtd/source/assets
	cp docker/website/Dockerfile.$(ARCH) $(WEBSITE_STAGE_DIR)/Dockerfile
	cp docker/website/nginx.conf $(WEBSITE_STAGE_DIR)
	cp docker/website/docker-entrypoint.sh $(WEBSITE_STAGE_DIR)
	cd $(WEBSITE_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/website:$(VERSION) $(DOCKER_PLATFORM) .

website-run: ## locally run the website; needs VERSION
	docker run -it -p 4000:4000 --rm --name website $(ECR)/website:$(VERSION)

website-export: ## export website to local host directory; needs VERSION
	-rm -Rf $(WEBSITE_EXPORT_DIR)
	mkdir -p $(WEBSITE_EXPORT_DIR)
	docker run -it --entrypoint /bin/bash  -v ./docker/website:/host -v $(WEBSITE_EXPORT_DIR):/output --rm --name website $(ECR)/website:$(VERSION) /host/export_dist.sh

website-live-update: website-export # Update the website docs in the S3 bucket and invalidate the CloudFront cache
	aws s3 sync $(WEBSITE_EXPORT_DIR)/jekyll/_site s3://$(PROJECT_WEBSITE_BUCKET) --delete
	aws cloudfront create-invalidation --distribution-id $(DISTRIBUTION_ID) --paths "/*"

website-cffunc-create: ## create the uri rewrite function for cloudfront
	aws cloudformation create-stack \
		--stack-name 	cffunc \
		--region		us-east-1 \
		--template-body file://cloudformation/cffunc.yml
	aws cloudformation wait stack-create-complete --region us-east-1 --stack-name cffunc

website-cffunc-describe: ## create the uri rewrite function for cloudfront
	aws cloudformation describe-stack-events --region us-east-1 --stack-name cffunc

website-cffunc-publish: ## publish the cloudfront function
	aws cloudfront publish-function --region us-east-1 --name cffunc-uri-rewrite --if-match $(CFFUNC_ETAG)

website-cffunc-delete: ## deletes the uri rewrite function for cloudfront
	aws cloudformation delete-stack --region us-east-1 --stack-name cffunc
	aws cloudformation wait stack-delete-complete --region us-east-1 --stack-name cffunc

website-create: ## create website stack
	aws cloudformation create-stack \
		--stack-name 	website \
		--template-body file://cloudformation/website.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		us-east-1 \
		--parameters 	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=HostedZoneId,ParameterValue=$(ZONE_ID) \
						ParameterKey=ProjectWebsiteBucket,ParameterValue=$(PROJECT_WEBSITE_BUCKET) \
						ParameterKey=CloudFrontFunctionARN,ParameterValue=$(CFFUNC_ID)
	aws cloudformation wait stack-create-complete --region us-east-1 --stack-name website

website-describe: ## status the creation/deletion of the website
	aws cloudformation describe-stack-events --region us-east-1 --stack-name website

website-delete: ## deletes the website stack
	aws cloudformation delete-stack --region us-east-1 --stack-name website
	aws cloudformation wait stack-delete-complete --region us-east-1 --stack-name website

website: ## build and update website
	make website-render-notebooks
	make website-docker
	make website-live-update

########################
# Certbot Targets
########################

certbot-build: ## create the certbot lambda zipfile
	-rm -Rf $(CERTBOT_STAGE_DIR)
	mkdir -p $(CERTBOT_STAGE_DIR)
	docker run -it --rm \
		-v $(CERTBOT_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.11 \
		-c "pip install --target /host/package certbot certbot-dns-route53"
	cp $(ROOT)/applications/certbot/main.py $(CERTBOT_STAGE_DIR)/package/
	cd $(CERTBOT_STAGE_DIR)/package/ && zip -r -q $(CERTBOT_STAGE_DIR)/certbot.zip . -x "/*__pycache__/*"

certbot-describe: ## status the creation/deletion of the certbot function
	aws cloudformation describe-stack-events --stack-name certbot

certbot-create: certbot-build ## creates the lambda certbot function
	aws s3 cp $(CERTBOT_STAGE_DIR)/certbot.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/certbot.zip
	aws cloudformation create-stack \
		--stack-name 	certbot \
		--template-body file://cloudformation/certbot.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters 	ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/certbot.zip
	aws cloudformation wait stack-create-complete --stack-name certbot

certbot-delete: ## deletes the lambda certbot function
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/certbot.zip
	aws cloudformation delete-stack --stack-name certbot
	aws cloudformation wait stack-delete-complete --stack-name certbot

########################
# Provisioner Targets
########################

provisioner-build: ## create the provisioner lambda zipfile
	-rm -Rf $(PROVISIONER_STAGE_DIR)
	mkdir -p $(PROVISIONER_STAGE_DIR)
	docker run -it --rm \
		-v $(PROVISIONER_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.11 \
		-c "pip install --target /host/package requests"
	cp $(ROOT)/applications/provisioner/main.py $(PROVISIONER_STAGE_DIR)/package/
	cp cloudformation/cluster.yml $(PROVISIONER_STAGE_DIR)/package/
	cd $(PROVISIONER_STAGE_DIR)/package/ && zip -r -q $(PROVISIONER_STAGE_DIR)/provisioner.zip . -x "/*__pycache__/*"

provisioner-describe: ## status the creation/deletion of the provisioner function
	aws cloudformation describe-stack-events --stack-name provisioner

provisioner-run: ## invokes the provisioner lambda function and displays the response
	echo '{"domain":"$(DOMAIN)","version":"$(VERSION)"}' > /tmp/payload.json
	aws lambda invoke --function-name Provisioner --payload fileb:///tmp/payload.json /tmp/response.json
	cat /tmp/response.json | jq

provisioner-create: ## creates the lambda provisioner function
	aws s3 cp $(PROVISIONER_STAGE_DIR)/provisioner.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation create-stack \
		--stack-name 	provisioner \
		--template-body file://cloudformation/provisioner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters 	ParameterKey=ContainerRegistry,ParameterValue=$(ECR) \
						ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation wait stack-create-complete --stack-name provisioner

provisioner-delete: ## deletes the lambda provisioner function
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation delete-stack --stack-name provisioner
	aws cloudformation wait stack-delete-complete --stack-name provisioner

########################
# Release Target
########################

release: buildenv-docker buildenv-docker-push manager-lock ams-lock ## tag and release sliderule; needs VERSION
	echo $(VERSION) > $(ROOT)/version.txt
	node $(ROOT)/clients/nodejs/utils/modpkg.js $(VERSION)
	git add $(ROOT)/clients/nodejs/sliderule/package.json
	cp $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/targets/slideruleearth/docker/manager/conda-linux-${ARCH}.lock
	git add $(ROOT)/targets/slideruleearth/docker/manager/conda-lock.yml
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-linux-${ARCH}.lock
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-lock.yml
	git add $(ROOT)/targets/slideruleearth/docker/sliderule/libdep-${ARCH}.lock
	git commit -m "Version $(VERSION)"
	git tag -a $(VERSION) -m "Version $(VERSION)"
	git push --tags && git push
	gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/web/rtd/developer_guide/release_notes/release_notes.html"
	make cluster-docker
	make cluster-docker-push

########################
# Clean Up Targets
########################

docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(ECR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule:$(VERSION)
	- docker image rm $(ECR)/ilb:$(VERSION)
	- docker image rm $(ECR)/proxy:$(VERSION)
	- docker image rm $(ECR)/monitor:$(VERSION)
	- docker image rm $(ECR)/monitor-agent:$(VERSION)
	- docker image rm $(ECR)/manager:$(VERSION)
	- docker image rm $(ECR)/ams:$(VERSION)
	- docker image rm $(ECR)/website:$(VERSION)
	- docker system prune -f

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)
	- cd $(ROOT)/docs && ./clean.sh
	- cd $(PYTHON_CLIENT_DIR) && ./clean.sh
	- cd $(ROOT)/applications/manager && ./clean.sh
	- cd $(ROOT)/applications/ams && ./clean.sh
	- find $(ROOT) -name ".cookies" -exec rm {} \;
	- rm -f utilities/report.pdf
	- rm -f utilities/report.xml

########################
# Test Targets
########################

TEST ?= $(ROOT)/targets/slideruleearth/test_runner.lua cloud
PYTEST ?= # specific pytests and additional pytest options can be supplied here
JEST ?= # specific jest tests and additional jest options can be supplied here
AMSTEST ?= # specific ams tests and additional pytest options can be supplied here
MNGRTEST ?= # specific manager tests and additional pytest options can be supplied here

MOSAICS_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_perf.lua
MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_serial_vs_batch_perf.lua
STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST  ?= $(ROOT)/datasets/pgc/systests/arcticdem_strips_serial_vs_batch_perf.lua

selftest: ## run the self test on the server code
	ASAN_OPTIONS="detect_leaks=1" LSAN_OPTIONS="suppressions=$(ROOT)/targets/slideruleearth/asan_leaks.supp" $(SLIDERULE_STAGE_DIR)/bin/sliderule $(TEST)

pytest: ## run python client tests; needs special conda environment
	cd $(PYTHON_CLIENT_DIR) && coverage run -m pytest --domain $(DOMAIN) --organization $(ORGANIZATION) $(PYTEST)
	cd $(PYTHON_CLIENT_DIR) && coverage report -m

jest: ## run node.js client self tests
	make -C $(NODEJS_CLIENT_DIR) test DOMAIN=$(DOMAIN) ORGANIZATION=$(ORGANIZATION) TEST=$(JEST)

amstest: ## run ams tests; needs special conda environment
	make -C $(ROOT)applications/ams test TEST=$(AMSTEST)

mngrtest: ## run manager tests; needs special conda environment
	make -C $(ROOT)applications/manager test TEST=$(MNGRTEST)

perfmtest: ## run mosaics performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_PERFORMANCE_TEST)

perfmsbtest: ## run mosaics serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

perfssbtest: ## run strips serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

########################
# Help Targets
########################

environment: ## echo the environment variables
	@echo INSTALLDIR: $(INSTALLDIR)
	@echo PROJECT_BUCKET: $(PROJECT_BUCKET)
	@echo PROJECT_FOLDER: $(PROJECT_FOLDER)
	@echo PROJECT_PUBLIC_BUCKET: $(PROJECT_PUBLIC_BUCKET)
	@echo PROJECT_WEBSITE_BUCKET: $(PROJECT_WEBSITE_BUCKET)
	@echo AWS_ACCOUNT: $(AWS_ACCOUNT)
	@echo AWS_REGION: $(AWS_REGION)
	@echo ECR: $(ECR)
	@echo MYIP: $(MYIP)
	@echo ENVVER: $(ENVVER)
	@echo ARCH: $(ARCH)
	@echo DOMAIN: $(DOMAIN)
	@echo ORGANIZATION: $(ORGANIZATION)
	@echo CLUSTER: $(CLUSTER)
	@echo VERSION: $(VERSION)
	@echo DOCKEROPTS: $(DOCKEROPTS)
	@echo BUILDX: $(BUILDX)
	@echo DOCKER_PLATFORM: $(DOCKER_PLATFORM)
	@echo USERCFG: $(USERCFG)
	@echo RUNNER: $(RUNNER)
	@echo DEBUG_CFG: $(DEBUG_CFG)
	@echo RELEASE_CFG: $(RELEASE_CFG)

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
