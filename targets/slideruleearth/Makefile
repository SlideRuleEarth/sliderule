# In order to complete the release process, the following must be installed and configured in your environment:
#     * GitHub command line client (gh)
#           See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#           The user must authenticate to GitHub via `gh auth login`
#     * AWS command line client (awscli)
#           See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#           The user must have up-to-date aws credentials
#     * Docker
#           The user must be logged into the AWS Elastic Container Registry
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
#     6. Execute the makefile target to release the code; X.Y.Z is the tag
#        $ make release RELEASE=vX.Y.Z
#
# To update the build environment for GitHub actions
#     1. Login to GitHub Container Registry; {my_github_key} is a file storing an access key created for your account
#        $ cat {my_github_key} | docker login ghcr.io -u jpswinski --password-stdin
#     2. Build the docker build environment on x86 machine (x86 is IMPORTANT to match GitHub actions environment)
#        $ make sliderule-buildenv-docker
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
MONITOR_STAGE_DIR = $(STAGE)/monitor
MONITOR_AGENT_STAGE_DIR = $(STAGE)/monitor-agent
AMS_STAGE_DIR = $(STAGE)/ams
CERTBOT_STAGE_DIR = $(STAGE)/certbot
PROVISIONER_STAGE_DIR = $(STAGE)/provisioner
AUTHENTICATOR_STAGE_DIR = $(STAGE)/authenticator
RECORDER_STAGE_DIR = $(STAGE)/recorder
WEBSITE_SOURCE_DIR = $(ROOT)/docs
WEBSITE_STAGE_DIR = $(STAGE)/website
WEBSITE_EXPORT_DIR = $(WEBSITE_STAGE_DIR)/dist
WEBSITE_ASSET_DIR = /data/web
PYTHON_CLIENT_DIR = $(ROOT)/clients/python
PYTHON_EXAMPLES_DIR = $(PYTHON_CLIENT_DIR)/examples
NODEJS_CLIENT_DIR = $(ROOT)/clients/nodejs

INSTALLDIR ?= $(SLIDERULE_STAGE_DIR)
DOCKEROPTS ?= # --progress=plain --no-cache

PROJECT_BUCKET = sliderule
PROJECT_FOLDER = cf
PROJECT_PUBLIC_BUCKET = sliderule-public
PROJECT_WEBSITE_BUCKET = sliderule-website
AWS_ACCOUNT = 742127912612
AWS_REGION = us-west-2
CONTAINER_REGISTRY = $(AWS_ACCOUNT).dkr.ecr.$(AWS_REGION).amazonaws.com

MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER ?= $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
ARCH ?= $(shell arch)
TIMESTAMP ?= $(shell date +%Y%m%d%H%M%S)

DOMAIN ?= slideruleearth.io
PUBLIC_CLUSTER ?= sliderule
CLUSTER ?= developers
VERSION ?= unstable

AUTHENTICATOR_URL ?= login.$(DOMAIN)
PROVISIONER_URL ?= provisioner.$(DOMAIN)
RECORDER_URL ?= recorder.$(DOMAIN)
ALERT_STREAM ?= recorder-firehose-alerts
TELEMETRY_STREAM ?= recorder-firehose-telemetry
JWT_ISSUER = https://$(AUTHENTICATOR_URL)
CORS_ALLOW_ORIGINS ?= "https://client.slideruleearth.io\,https://testsliderule.org"
HOSTED_ZONE_ID = $(shell aws route53 list-hosted-zones-by-name \
  --dns-name $(DOMAIN) \
  --query "HostedZones[?Name=='$(DOMAIN).'].Id" \
  --output text | sed 's|/hostedzone/||')
CERTIFICATE_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name certificate \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`CertificateArn`].OutputValue' \
  --output text)
DESTROY_LAMBDA_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name provisioner \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`DestroyLambdaArn`].OutputValue' \
  --output text)
SCHEDULER_LAMBDA_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name provisioner \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`SchedulerLambdaArn`].OutputValue' \
  --output text)

########################
# Local Build Targets
########################

USERCFG ?= # e.g. -DSKIP_STATIC_ANALYSIS=1
RUNNER ?= # e.g. valgrind
BUILD_CONFIG ?= release
BUILD_CMD ?= sh -c "cd $(ROOT)/targets/slideruleearth && make config-$(BUILD_CONFIG) && make"

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

all: ## build the server using the local configuration
	make -j8 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)

config-debug: prep ## configure the server for running locally with debug symbols, optimizations off, static analysis, and address sanitizer
	cd $(SLIDERULE_BUILD_DIR) && $(CLANG_CFG) && cmake $(DEBUG_CFG) -DMAX_FREE_STACK_SIZE=1 -DENABLE_TRACING=ON $(ROOT)

config-release: prep ## configure server to run a release version locally
	cd $(SLIDERULE_BUILD_DIR) && cmake $(RELEASE_CFG) -DMAX_FREE_STACK_SIZE=1 $(ROOT)

run: ## run the server locally
	LOG_FORMAT=FMT_TEXT \
	TRUSTED_ENVIRONMENT=true \
	IPV4=$(MYIP) \
	ENVIRONMENT_VERSION=$(ENVVER) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	ORCHESTRATOR=http://127.0.0.1:8050 \
	CLUSTER=local \
	AMS=http://127.0.0.1:9082 \
	CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
	$(RUNNER) $(INSTALLDIR)/bin/sliderule $(ROOT)/targets/slideruleearth/server.lua

build: ## build the code using the build environment docker container
	docker run \
		$(DOCKEROPTS) \
		--network host \
		--user $(shell id -u):$(shell id -g) \
		-v $(shell realpath ~/.ssh):/root/.ssh \
		-v $(ROOT):$(ROOT) \
		-v /data:/data \
		-e MYIP=$(MYIP) \
		-e ROOT=$(ROOT) \
		-e GITNAME="$(shell git config user.name)" \
		-e GITEMAIL="$(shell git config user.email)" \
		-e LOG_FORMAT=FMT_TEXT \
		-e TRUSTED_ENVIRONMENT=true \
		-e IPV4=$(MYIP) \
		-e ENVIRONMENT_VERSION=$(ENVVER) \
		-e PROJECT_BUCKET=$(PROJECT_BUCKET) \
		-e PROJECT_FOLDER=$(PROJECT_FOLDER) \
		-e ORCHESTRATOR=http://127.0.0.1:8050 \
		-e CLUSTER=local \
		-e AMS=http://127.0.0.1:9082 \
		-e CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
		--rm \
		--name buildenv \
		$(CONTAINER_REGISTRY)/sliderule-buildenv:latest \
		$(BUILD_CMD)

########################
# Docker Build Targets
########################

sliderule-buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cp docker/sliderule/.bashrc $(BUILDENV_STAGE_DIR)
	cd $(BUILDENV_STAGE_DIR); docker build $(DOCKEROPTS) -t $(CONTAINER_REGISTRY)/sliderule-buildenv:latest .
	docker run -it -v ./docker/sliderule:/host --rm --name buildenv $(CONTAINER_REGISTRY)/sliderule-buildenv:latest /bin/bash -c "cat /libdep.lock > /host/libdep-$(ARCH).lock"

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SLIDERULE_STAGE_DIR)
	mkdir -p $(SLIDERULE_STAGE_DIR)
	rsync -a $(ROOT) $(SLIDERULE_STAGE_DIR) --exclude build --exclude stage
	cp docker/sliderule/Dockerfile.runtime $(SLIDERULE_STAGE_DIR)/Dockerfile
	cp docker/sliderule/docker-entrypoint.sh $(SLIDERULE_STAGE_DIR)/
	cd $(SLIDERULE_STAGE_DIR); docker build $(DOCKEROPTS) --build-arg repo=$(CONTAINER_REGISTRY) -t $(CONTAINER_REGISTRY)/sliderule:$(VERSION) .

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/ilb/Dockerfile $(ILB_STAGE_DIR)
	cp $(ROOT)/applications/ilb/haproxy.cfg $(ILB_STAGE_DIR)
	cp $(ROOT)/applications/ilb/orchestrator.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/applications/ilb/analyzer.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/packages/core/extensions/json.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/packages/core/extensions/prettyprint.lua $(ILB_STAGE_DIR)
	cd $(ILB_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(CONTAINER_REGISTRY)/ilb:$(VERSION) .

monitor-docker: ## build monitor docker image; needs VERSION
	-rm -Rf $(MONITOR_STAGE_DIR)
	mkdir -p $(MONITOR_STAGE_DIR)
	cp -R docker/monitor/rootfs/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	cd $(MONITOR_STAGE_DIR); docker build $(DOCKEROPTS) -t $(CONTAINER_REGISTRY)/monitor:$(VERSION) .

monitor-agent-docker: ## build monitor agent docker image; needs VERSION
	-rm -Rf $(MONITOR_AGENT_STAGE_DIR)
	mkdir -p $(MONITOR_AGENT_STAGE_DIR)
	cp -R docker/monitor-agent/rootfs/* $(MONITOR_AGENT_STAGE_DIR)
	cp docker/monitor-agent/Dockerfile.$(ARCH) $(MONITOR_AGENT_STAGE_DIR)/Dockerfile
	cd $(MONITOR_AGENT_STAGE_DIR); docker build $(DOCKEROPTS) -t $(CONTAINER_REGISTRY)/monitor-agent:$(VERSION) .

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
	cd $(AMS_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(CONTAINER_REGISTRY)/ams:$(VERSION) .

########################
# Cluster Targets
########################

PUBLIC_IP ?= 127.0.0.1
IS_PUBLIC ?= false
NODE_CAPACITY ?= 1
TTL ?= 60
CLUSTER_PARMS = '{\
	"version": "$(VERSION)",\
	"is_public": $(IS_PUBLIC),\
	"domain": "$(DOMAIN)",\
	"cluster": "$(CLUSTER)",\
	"node_capacity": $(NODE_CAPACITY),\
	"ttl": $(TTL)
}'

cluster-docker: ## build all cluster docker images
	make sliderule-docker
	make ilb-docker
	make monitor-docker
	make monitor-agent-docker
	make ams-docker

cluster-docker-push: ## push all cluster images to container registries (and docker compose files to S3)
	docker push $(CONTAINER_REGISTRY)/sliderule:$(VERSION)
	docker push $(CONTAINER_REGISTRY)/ilb:$(VERSION)
	docker push $(CONTAINER_REGISTRY)/monitor:$(VERSION)
	docker push $(CONTAINER_REGISTRY)/monitor-agent:$(VERSION)
	docker push $(CONTAINER_REGISTRY)/ams:$(VERSION)

cluster-deploy: ## invokes the provisioner *deploy* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --is_public $(IS_PUBLIC) --node_capacity $(NODE_CAPACITY) --ttl $(TTL) --version $(VERSION) --commands deploy

cluster-destroy: ## invokes the provisioner *destroy* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands destroy

cluster-status: ## invokes the provisioner *status* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands status

cluster-events: ## invokes the provisioner *events* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands events

cluster-report: ## invokes the provisioner *report* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands report

cluster-version: ## calls the cluster version API and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands version

cluster-deploy-candidate: ## deploy candidate public cluster
	@echo "Deploying cluster <sliderule-$(TIMESTAMP)>"
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster sliderule-$(TIMESTAMP) --is_public true --node_capacity 7 --ttl 525600 --version latest --commands deploy

cluster-go-live: ## switch dns entry of public cluster
	aws cloudformation deploy \
		--stack-name 			dns \
		--template-file 		$(ROOT)/applications/provisioner/dns.yml \
		--capabilities 			CAPABILITY_NAMED_IAM \
		--region 				$(AWS_REGION) \
		--parameter-overrides 	Domain=$(DOMAIN) \
								Subdomain=$(PUBLIC_CLUSTER) \
								PublicIp=$(PUBLIC_IP)

cluster-take-offline: ## destroy dns entry of public cluster
	aws cloudformation delete-stack --stack-name dns

cluster-direct-deploy: ## directly deploys cluster via cloud formation
	aws cloudformation create-stack \
		--stack-name 	$(CLUSTER)-cluster \
		--template-body file://$(ROOT)/applications/provisioner/cluster.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=Version,ParameterValue=$(VERSION) \
						ParameterKey=IsPublic,ParameterValue=$(IS_PUBLIC) \
						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=Cluster,ParameterValue=$(CLUSTER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=ProjectPublicBucket,ParameterValue=$(PROJECT_PUBLIC_BUCKET) \
						ParameterKey=DestroyLambdaArn,ParameterValue=$(DESTROY_LAMBDA_ARN) \
						ParameterKey=SchedulerLambdaArn,ParameterValue=$(SCHEDULER_LAMBDA_ARN) \
						ParameterKey=ContainerRegistry,ParameterValue=$(CONTAINER_REGISTRY) \
						ParameterKey=NodeCapacity,ParameterValue=$(NODE_CAPACITY) \
						ParameterKey=TTL,ParameterValue=$(TTL) \
						ParameterKey=JwtIssuer,ParameterValue=$(JWT_ISSUER) \
						ParameterKey=AlertStream,ParameterValue=$(ALERT_STREAM) \
						ParameterKey=TelemetryStream,ParameterValue=$(TELEMETRY_STREAM)
	aws cloudformation wait stack-create-complete --stack-name $(CLUSTER)-cluster

cluster-direct-describe: ## directly status cluster created via cloud formation
	aws cloudformation describe-stack-events --stack-name $(CLUSTER)-cluster

cluster-direct-destroy: ## directly destroys cluster via cloud formation
	aws cloudformation delete-stack --stack-name $(CLUSTER)-cluster
	aws cloudformation wait stack-delete-complete --stack-name $(CLUSTER)-cluster

########################
# Test Runner Targets
########################
#	- Requires the provisioner to be created
#	- The provisioner provides lambdas that are used to destroy the testrunner stack
#	- The testrunner automatically destroys itself after one hour

BRANCH ?= main

testrunner-create: ## invokes the provisioner lambda function and displays the response
	aws cloudformation create-stack \
		--stack-name 	testrunner \
		--template-body file://$(ROOT)/applications/provisioner/testrunner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=ContainerRegistry,ParameterValue=$(CONTAINER_REGISTRY) \
						ParameterKey=DestroyLambdaArn,ParameterValue=$(DESTROY_LAMBDA_ARN) \
						ParameterKey=SchedulerLambdaArn,ParameterValue=$(SCHEDULER_LAMBDA_ARN) \
						ParameterKey=DeployDate,ParameterValue=$(TIMESTAMP) \
						ParameterKey=Branch,ParameterValue=$(BRANCH)
	aws cloudformation wait stack-create-complete --stack-name testrunner

testrunner-describe: ## status the creation/deletion of the test runner
	aws cloudformation describe-stack-events --stack-name testrunner

testrunner-delete: ## deletes the test runner and any lingering event bridge rules (debug only)
	aws cloudformation delete-stack --stack-name testrunner
	aws cloudformation wait stack-delete-complete --stack-name testrunner
	- aws events remove-targets --rule testrunner-auto-delete --ids 1
	- aws events delete-rule --name testrunner-auto-delete

########################
# Recorder Targets
########################

recorder-build: ## builds the recorder lambda package zipfile
	-rm -Rf $(RECORDER_STAGE_DIR)
	mkdir -p $(RECORDER_STAGE_DIR)
	cp $(ROOT)/applications/recorder/requirements.txt $(RECORDER_STAGE_DIR)/
	docker run -it --rm \
		-v $(RECORDER_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.13 \
		-c "pip install --target /host/package -r /host/requirements.txt"
	cp $(ROOT)/applications/recorder/main.py $(RECORDER_STAGE_DIR)/package/
	cd $(RECORDER_STAGE_DIR)/package/ && zip -r -q $(RECORDER_STAGE_DIR)/recorder.zip . -x "/*__pycache__/*"

recorder-create: recorder-build  ## creates the lambda recorder functions
	aws s3 cp $(RECORDER_STAGE_DIR)/recorder.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/recorder.zip
	aws cloudformation create-stack \
		--stack-name 	recorder \
		--template-body file://$(ROOT)/applications/recorder/recorder.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=RecorderUrl,ParameterValue=$(RECORDER_URL) \
						ParameterKey=JwtIssuer,ParameterValue=$(JWT_ISSUER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=AlertStream,ParameterValue=$(ALERT_STREAM) \
						ParameterKey=TelemetryStream,ParameterValue=$(TELEMETRY_STREAM) \
						ParameterKey=CorsAllowOrigins,ParameterValue=$(CORS_ALLOW_ORIGINS) \
						ParameterKey=CertificateArn,ParameterValue=$(CERTIFICATE_ARN) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/recorder.zip
	aws cloudformation wait stack-create-complete --stack-name recorder

recorder-describe: ## status the creation/deletion of the recorder functions
	aws cloudformation describe-stack-events --stack-name recorder

recorder-delete: ## deletes the lambda recorder functions
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/recorder.zip
	aws cloudformation delete-stack --stack-name recorder
	aws cloudformation wait stack-delete-complete --stack-name recorder

########################
# Authenticator Targets
########################

GITHUB_ORG ?= SlideRuleEarth
GITHUB_OAUTH_APP_CLIENT_ID ?= Ov23liTqpZCtvFofQEFE
CLIENT_SECRET_NAME ?= sliderule/github-oauth-client-secret
ALLOWED_REDIRECT_HOSTS ?= "slideruleearth.io testsliderule.org localhost 127.0.0.1"

authenticator-build: ## builds the authenticator lambda package zipfile
	-rm -Rf $(AUTHENTICATOR_STAGE_DIR)
	mkdir -p $(AUTHENTICATOR_STAGE_DIR)
	cp $(ROOT)/applications/authenticator/requirements.txt $(AUTHENTICATOR_STAGE_DIR)/
	docker run -it --rm \
		-v $(AUTHENTICATOR_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.13 \
		-c "pip install --target /host/package -r /host/requirements.txt"
	cp $(ROOT)/applications/authenticator/github_oauth.py $(AUTHENTICATOR_STAGE_DIR)/package/
	cd $(AUTHENTICATOR_STAGE_DIR)/package/ && zip -r -q $(AUTHENTICATOR_STAGE_DIR)/authenticator.zip . -x "/*__pycache__/*"

authenticator-create: authenticator-build  ## creates the lambda authenticator functions
	aws s3 cp $(AUTHENTICATOR_STAGE_DIR)/authenticator.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/authenticator.zip
	aws cloudformation create-stack \
		--stack-name 	authenticator \
		--template-body file://$(ROOT)/applications/authenticator/authenticator.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=AuthenticatorUrl,ParameterValue=$(AUTHENTICATOR_URL) \
						ParameterKey=GitHubOrg,ParameterValue=$(GITHUB_ORG) \
						ParameterKey=GitHubClientId,ParameterValue=$(GITHUB_OAUTH_APP_CLIENT_ID) \
						ParameterKey=ClientSecretName,ParameterValue=$(CLIENT_SECRET_NAME) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
						ParameterKey=CertificateArn,ParameterValue=$(CERTIFICATE_ARN) \
						ParameterKey=AllowedRedirectHosts,ParameterValue=$(ALLOWED_REDIRECT_HOSTS) \
						ParameterKey=CorsAllowOrigins,ParameterValue=$(CORS_ALLOW_ORIGINS) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/authenticator.zip
	aws cloudformation wait stack-create-complete --stack-name authenticator

authenticator-describe: ## status the creation/deletion of the authenticator functions
	aws cloudformation describe-stack-events --stack-name authenticator

authenticator-output: ## get the outputs from the creation of the authenticator stack
	aws cloudformation describe-stacks --stack-name authenticator --output text

authenticator-delete: ## deletes the lambda authenticator functions
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/authenticator.zip
	aws cloudformation delete-stack --stack-name authenticator
	aws cloudformation wait stack-delete-complete --stack-name authenticator

authenticator-init: ## put GitHub client secret in AWS secrets manager (one-time setup)
	@read -s -p "Enter GitHub OAuth Client Secret: " GITHUB_OAUTH_APP_CLIENT_SECRET; echo ""; \
	aws secretsmanager create-secret \
		--name $(CLIENT_SECRET_NAME) \
		--secret-string "$$GITHUB_OAUTH_APP_CLIENT_SECRET" \
		--description "GitHub OAuth Client Secret for SlideRule"

authenticator-public-key: ## get the public pem key for JWT verification
	mkdir -p /etc/haproxy/pem
	curl $(JWT_ISSUER)/auth/github/pem > /etc/haproxy/pem/pubkey.pem

########################
# Provisioner Targets
########################

provisioner-build: ## create the provisioner lambda zipfile
	-rm -Rf $(PROVISIONER_STAGE_DIR)
	mkdir -p $(PROVISIONER_STAGE_DIR)
	cp $(ROOT)/applications/provisioner/requirements.txt $(PROVISIONER_STAGE_DIR)/
		docker run -it --rm \
		-v $(PROVISIONER_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.13 \
		-c "pip install --target /host/package requests croniter"
	cp $(ROOT)/applications/provisioner/provisioner.py $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/manager.py $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/cluster.yml $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/testrunner.yml $(PROVISIONER_STAGE_DIR)/package/
	cd $(PROVISIONER_STAGE_DIR)/package/ && zip -r -q $(PROVISIONER_STAGE_DIR)/provisioner.zip . -x "/*__pycache__/*"

provisioner-create: provisioner-build ## creates the lambda provisioner functions
	aws s3 cp $(PROVISIONER_STAGE_DIR)/provisioner.zip s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation create-stack \
		--stack-name 	provisioner \
		--template-body file://$(ROOT)/applications/provisioner/provisioner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=ProvisionerUrl,ParameterValue=$(PROVISIONER_URL) \
						ParameterKey=JwtIssuer,ParameterValue=$(JWT_ISSUER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=ProjectPublicBucket,ParameterValue=$(PROJECT_PUBLIC_BUCKET) \
						ParameterKey=ContainerRegistry,ParameterValue=$(CONTAINER_REGISTRY) \
						ParameterKey=CorsAllowOrigins,ParameterValue=$(CORS_ALLOW_ORIGINS) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
						ParameterKey=CertificateArn,ParameterValue=$(CERTIFICATE_ARN) \
						ParameterKey=AlertStream,ParameterValue=$(ALERT_STREAM) \
						ParameterKey=TelemetryStream,ParameterValue=$(TELEMETRY_STREAM) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation wait stack-create-complete --stack-name provisioner

provisioner-describe: ## status the creation/deletion of the provisioner functions
	aws cloudformation describe-stack-events --stack-name provisioner

provisioner-delete: ## deletes the lambda provisioner functions
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/provisioner.zip
	aws cloudformation delete-stack --stack-name provisioner
	aws cloudformation wait stack-delete-complete --stack-name provisioner

########################
# Certificate Targets
########################

CERTIFICATE_DOMAINS ?= '*.$(DOMAIN)'

certificate-create: ## creates the certificate
	aws cloudformation create-stack \
		--stack-name 	certificate \
		--template-body file://$(ROOT)/applications/authenticator/certificate.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters 	ParameterKey=CertificateDomains,ParameterValue=$(CERTIFICATE_DOMAINS) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID)
	aws cloudformation wait stack-create-complete --stack-name certbot

certificate-describe: ## status the creation/deletion of the certificate
	aws cloudformation describe-stack-events --stack-name certificate

certificate-delete: ## deletes the certificate
	aws cloudformation delete-stack --stack-name certificate
	aws cloudformation wait stack-delete-complete --stack-name certificate

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
		--template-body file://$(ROOT)/applications/certbot/certbot.yml \
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
# Website Targets
########################

DISTRIBUTION_ID = $(shell aws cloudfront list-distributions \
	--query "DistributionList.Items[?Aliases.Items[0]=='$(DOMAIN)'].Id" \
	--output text)

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
	cd $(WEBSITE_STAGE_DIR) && docker build -t $(CONTAINER_REGISTRY)/website:$(VERSION) .

website-run: ## locally run the website; needs VERSION
	docker run -it -p 4000:4000 --rm --name website $(CONTAINER_REGISTRY)/website:$(VERSION)

website-export: ## export website to local host directory; needs VERSION
	-rm -Rf $(WEBSITE_EXPORT_DIR)
	mkdir -p $(WEBSITE_EXPORT_DIR)
	docker run -it --entrypoint /bin/bash  -v ./docker/website:/host -v $(WEBSITE_EXPORT_DIR):/output --rm --name website $(CONTAINER_REGISTRY)/website:$(VERSION) /host/export_dist.sh

website-live-update: website-export # Update the website docs in the S3 bucket and invalidate the CloudFront cache
	aws s3 sync $(WEBSITE_EXPORT_DIR)/jekyll/_site s3://$(PROJECT_WEBSITE_BUCKET) --delete
	aws cloudfront create-invalidation --distribution-id $(DISTRIBUTION_ID) --paths "/*"

website-cffunc-create: ## create the uri rewrite function for cloudfront
	aws cloudformation create-stack \
		--stack-name 	cffunc \
		--region		us-east-1 \
		--template-body file://cloudfront/cffunc.yml
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
		--template-body file://cloudfront/website.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		us-east-1 \
		--parameters 	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
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
# Release Target
########################

RELEASE ?= # vX.Y.Z

release: ## tag and release sliderule; needs RELEASE
	make sliderule-buildenv-docker
	make ams-lock
	echo $(RELEASE) > $(ROOT)/version.txt
	node $(ROOT)/clients/nodejs/utils/modpkg.js $(RELEASE)
	git add $(ROOT)/clients/nodejs/sliderule/package.json
	cp $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-linux-${ARCH}.lock
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-lock.yml
	git add $(ROOT)/targets/slideruleearth/docker/sliderule/libdep-${ARCH}.lock
	git commit -m "Version $(RELEASE)"
	git tag -a $(RELEASE) -m "Version $(RELEASE)"
	git push --tags && git push
	gh release create $(RELEASE) -t $(RELEASE) --notes "see https://slideruleearth.io/web/rtd/developer_guide/release_notes/release_notes.html"
	make cluster-docker VERSION=latest
	make cluster-docker-push VERSION=latest
	docker push $(CONTAINER_REGISTRY)/sliderule-buildenv:latest

########################
# Clean Up Targets
########################

docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(CONTAINER_REGISTRY)/sliderule-buildenv:latest
	- docker image rm $(CONTAINER_REGISTRY)/sliderule:$(VERSION)
	- docker image rm $(CONTAINER_REGISTRY)/ilb:$(VERSION)
	- docker image rm $(CONTAINER_REGISTRY)/monitor:$(VERSION)
	- docker image rm $(CONTAINER_REGISTRY)/monitor-agent:$(VERSION)
	- docker image rm $(CONTAINER_REGISTRY)/ams:$(VERSION)
	- docker image rm $(CONTAINER_REGISTRY)/website:$(VERSION)
	- docker system prune -f

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)
	- cd $(ROOT)/docs && ./clean.sh
	- cd $(PYTHON_CLIENT_DIR) && ./clean.sh
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

MOSAICS_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_perf.lua
MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_serial_vs_batch_perf.lua
STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST  ?= $(ROOT)/datasets/pgc/systests/arcticdem_strips_serial_vs_batch_perf.lua

selftest: ## run the self test on the server code
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	TRUSTED_ENVIRONMENT=true \
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(TEST)

pytest: ## run python client tests; needs special conda environment
	cd $(PYTHON_CLIENT_DIR) && coverage run -m pytest --domain $(DOMAIN) --organization $(CLUSTER) $(PYTEST)
	cd $(PYTHON_CLIENT_DIR) && coverage report -m

jest: ## run node.js client self tests
	make -C $(NODEJS_CLIENT_DIR) test DOMAIN=$(DOMAIN) ORGANIZATION=$(CLUSTER) TEST=$(JEST)

amstest: ## run ams tests; needs special conda environment
	make -C $(ROOT)applications/ams test TEST=$(AMSTEST)

perfmtest: ## run mosaics performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_PERFORMANCE_TEST)

perfmsbtest: ## run mosaics serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

perfssbtest: ## run strips serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

LAMBDA_PARMS ?= # json encoded parameters
LAMBDA_NAME ?= # function to invoke

lambdatest:
	echo $(LAMBDA_PARMS) > /tmp/payload.json
	aws lambda invoke --function-name $(LAMBDA_NAME) --payload fileb:///tmp/payload.json /tmp/response.json
	cat /tmp/response.json | jq

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
	@echo CONTAINER_REGISTRY: $(CONTAINER_REGISTRY)
	@echo MYIP: $(MYIP)
	@echo ENVVER: $(ENVVER)
	@echo ARCH: $(ARCH)
	@echo DOMAIN: $(DOMAIN)
	@echo CLUSTER: $(CLUSTER)
	@echo VERSION: $(VERSION)
	@echo DOCKEROPTS: $(DOCKEROPTS)
	@echo USERCFG: $(USERCFG)
	@echo RUNNER: $(RUNNER)
	@echo DEBUG_CFG: $(DEBUG_CFG)
	@echo RELEASE_CFG: $(RELEASE_CFG)
	@echo CERTIFICATE_ARN: $(CERTIFICATE_ARN)

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
