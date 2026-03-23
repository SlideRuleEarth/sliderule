# SlideruleEarth Makefile
#
#	|-------------------------------|
#   |   Certbot: ssl certificates   |
#   |-------------------------------|
#                  	|
#	|-------------------------------|
#   |    Authenticator: login       |
#   |-------------------------------|
#					|
#	|-------------------------------|
#   |    Provisioner: resources     |
#   |-------------------------------|
#					|
#	|-------------------------------|
#   |  Cluster: science processing  |
#   |-------------------------------|
#			|				 |
#	|---------------| |-------------|
#   | Python Client | | Web Client  |
#   |---------------| |-------------|
#
# Typical workflows
#
#	- Develop the server code using a debug version
#		$ make config-debug
#		$ make
#		$ make selftest
#
#	- Run and test a release version of the server code
#		$ make config-release
#		$ make
#		$ make run # in its own shell
#		$ make sliderule-test
#		$ make ams-test
#
#	- Run and test authenticator
#		$ make authenticator-test|delete|create
#
#	- Run and test provisioner
#		$ make provisioner-test|delete|create
#
#	- Full deployment (from scratch; must be performed in this order)
#		$ make certbot-create
#		$ make authenticator-create
#		$ make provisioner-create
#		$ make cluster-docker
#		$ make cluster-docker-push
#		$ make cluster-deploy
#
# The following prerequisites must be installed and configured in your environment:
#
#   - GitHub command line client (gh)
#       See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#       The user must authenticate to GitHub via `gh auth login`
#   - AWS command line client (awscli)
#       See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#       The user must have up-to-date aws credentials
#   - Docker
#       The user must be logged into the AWS Elastic Container Registry
#   - Conda and Conda-Lock
#		The Python tests use Conda to manage the runtime environment for the test execution
#       The Python base image used for the container runtime environment uses conda-lock to create the conda dependency file
#	- AWS credentials with permission to access S3 and AWS Elastic Container Registry (ECR), and log in to ECR
# 		$ aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
#   - GitHub credentials
#       $ gh auth login
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
RUNNER_STAGE_DIR = $(STAGE)/runner
AUTHENTICATOR_STAGE_DIR = $(STAGE)/authenticator
RECORDER_STAGE_DIR = $(STAGE)/recorder
WEBSITE_SOURCE_DIR = $(ROOT)/docs
DOCUMENTATION_STAGE_DIR = $(STAGE)/documentation
WEBSITE_EXPORT_DIR = $(DOCUMENTATION_STAGE_DIR)/dist
DOCUMENTATION_ASSET_DIR = /data/web
PYTHON_CLIENT_DIR = $(ROOT)/clients/python
PYTHON_EXAMPLES_DIR = $(PYTHON_CLIENT_DIR)/examples
NODEJS_CLIENT_DIR = $(ROOT)/clients/nodejs

INSTALLDIR ?= $(SLIDERULE_STAGE_DIR)
DOCKEROPTS ?= # --progress=plain --no-cache

IPV4 ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER ?= $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
ARCH ?= $(shell arch)
TIMESTAMP ?= $(shell date +%Y%m%d%H%M%S)

DOMAIN ?= slideruleearth.io
BASE_DOMAIN ?= $(basename $(DOMAIN))
PUBLIC_CLUSTER ?= sliderule
CLUSTER ?= developers
VERSION ?= unstable
BRANCH ?= main
SUPPORT_EMAIL ?= support@mail.slideruleearth.io
CORS_ALLOW_ORIGINS ?= "https://client.slideruleearth.io\,https://client.testsliderule.org"

PROJECT_BUCKET = sliderule
PROJECT_FOLDER = cf
PROJECT_PUBLIC_BUCKET = sliderule-public
PROJECT_WEBSITE_BUCKET = slideruleearth-documentation-dot
AWS_ACCOUNT = 742127912612
AWS_REGION = us-west-2
CONTAINER_REGISTRY = $(AWS_ACCOUNT).dkr.ecr.$(AWS_REGION).amazonaws.com

PUBLIC_IP ?= 127.0.0.1
IS_PUBLIC ?= false
NODE_CAPACITY ?= 1
TTL ?= 60

AUTHENTICATOR_STACKNAME = authenticator-$(BASE_DOMAIN)
PROVISIONER_STACKNAME = provisioner-$(BASE_DOMAIN)
RUNNER_STACKNAME = runner-$(BASE_DOMAIN)
CERTBOT_STACKNAME = certbot-$(BASE_DOMAIN)
RECORDER_STACKNAME = recorder # not scoped to domain
TESTRUNNER_STACKNAME = testrunner # not scoped to domain

AUTHENTICATOR_HOSTNAME = login.$(DOMAIN)
PROVISIONER_HOSTNAME = provisioner.$(DOMAIN)
RUNNER_HOSTNAME = runner.$(DOMAIN)
DOCUMENTATION_HOSTNAME = docs.$(DOMAIN)

AUTHENTICATOR_ZIPFILE = $(AUTHENTICATOR_STACKNAME).zip
PROVISIONER_ZIPFILE = $(PROVISIONER_STACKNAME).zip
RUNNER_ZIPFILE = $(RUNNER_STACKNAME).zip
CERTBOT_ZIPFILE = $(CERTBOT_STACKNAME).zip

ALERT_STREAM = recorder-firehose-alerts
TELEMETRY_STREAM = recorder-firehose-telemetry

DOCUMENTATION_CONDA_ENV ?= sliderule_notebook # clients/python/examples/environment.yml
DOCUMENTATION_CONDA_ACTIVATE = source "$(shell conda info --base)/etc/profile.d/conda.sh" && conda activate $(DOCUMENTATION_CONDA_ENV)

JWT_ISSUER = https://$(AUTHENTICATOR_HOSTNAME)
AFFILIATES_FILENAME = "affiliates.json"
GITHUB_ORG ?= SlideRuleEarth
GITHUB_CLIENT_SECRET_NAME ?= $(DOMAIN)/github-oauth-client
TRUSTED_REDIRECT_HOSTS ?= "slideruleearth.io testsliderule.org localhost 127.0.0.1"
THIRD_PARTY_REDIRECT_HOSTS ?= "claude.ai"

HOSTED_ZONE_ID = $(shell aws route53 list-hosted-zones-by-name \
  --dns-name $(DOMAIN) \
  --query "HostedZones[?Name=='$(DOMAIN).'].Id" \
  --output text | sed 's|/hostedzone/||')
CERTIFICATE_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name $(CERTBOT_STACKNAME) \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`CertificateArn`].OutputValue' \
  --output text)
DESTROY_LAMBDA_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name $(PROVISIONER_STACKNAME) \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`DestroyLambdaArn`].OutputValue' \
  --output text)
SCHEDULE_LAMBDA_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name $(PROVISIONER_STACKNAME) \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`ScheduleLambdaArn`].OutputValue' \
  --output text)
ALERT_EMAIL = $(shell aws secretsmanager get-secret-value \
   --secret-id $(DOMAIN)/secrets \
   --query SecretString \
   --output text | jq -r .alert_email)
JWT_SIGNING_KEY_ARN = $(shell aws cloudformation describe-stacks \
  --stack-name $(AUTHENTICATOR_STACKNAME) \
  --region us-west-2 \
  --query 'Stacks[0].Outputs[?OutputKey==`JWTSigningKeyArn`].OutputValue' \
  --output text)
DISTRIBUTION_ID = $(shell aws cloudfront list-distributions \
  --query "DistributionList.Items[?Aliases.Items[0]=='$(DOCUMENTATION_HOSTNAME)'].Id" \
  --output text)

USERCFG ?= # e.g. -DSKIP_STATIC_ANALYSIS=1
RUNNER ?= # e.g. valgrind
BUILD_CONFIG ?= release
BUILD_CMD ?= sh -c "cd $(ROOT)/targets/slideruleearth && make config-$(BUILD_CONFIG) && make"
ARGS ?=

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

RELEASE ?= # vX.Y.Z
TEST ?= $(ROOT)/targets/slideruleearth/test_runner.lua
PYTEST ?= # specific pytests and additional pytest options can be supplied here
JEST ?= # specific jest tests and additional jest options can be supplied here

########################
# Local Build Targets
########################

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
	IPV4=$(IPV4) \
	ENVIRONMENT_VERSION=$(ENVVER) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	PROJECT_REGION=$(AWS_REGION) \
	ORCHESTRATOR=http://127.0.0.1:8050 \
	CLUSTER=local \
	DOMAIN=localhost \
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
		-e IPV4=$(IPV4) \
		-e ROOT=$(ROOT) \
		-e GITNAME="$(shell git config user.name)" \
		-e GITEMAIL="$(shell git config user.email)" \
		-e LOG_FORMAT=FMT_TEXT \
		-e ENVIRONMENT_VERSION=$(ENVVER) \
		-e PROJECT_BUCKET=$(PROJECT_BUCKET) \
		-e PROJECT_FOLDER=$(PROJECT_FOLDER) \
		-e PROJECT_REGION=$(AWS_REGION) \
		-e ORCHESTRATOR=http://127.0.0.1:8050 \
		-e CLUSTER=local \
		-e DOMAIN=localhost \
		-e AMS=http://127.0.0.1:9082 \
		-e CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
		--rm \
		--name buildenv \
		$(CONTAINER_REGISTRY)/sliderule-buildenv:latest \
		$(BUILD_CMD)

job: ## execute the job runner locally
	LOG_FORMAT=FMT_TEXT \
	IPV4=$(IPV4) \
	ENVIRONMENT_VERSION=$(ENVVER) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	PROJECT_REGION=$(AWS_REGION) \
	CLUSTER=local \
	CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(ROOT)/targets/slideruleearth/job_runner.lua $(ARGS) /tmp/result.json

python: ## update python client in sliderule environment
	conda run -n sliderule --no-capture-output \
	sh -c 'cd $(ROOT)/clients/python && pip install .'

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

ams-run:
	cd $(ROOT)/applications/ams && \
	conda run -n ams --no-capture-output \
	flask --app ams run --debug --port 9082

########################
# Cluster Targets
########################

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
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --is_public $(IS_PUBLIC) --node_capacity $(NODE_CAPACITY) --ttl $(TTL) --version $(VERSION) --commands deploy'

cluster-extend: ## invokes the provisioner *extend* lambda function and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --is_public $(IS_PUBLIC) --node_capacity $(NODE_CAPACITY) --ttl $(TTL) --version $(VERSION) --commands deploy'

cluster-destroy: ## invokes the provisioner *destroy* lambda function and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands destroy'

cluster-status: ## invokes the provisioner *status* lambda function and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands status --concise --asjson'

cluster-events: ## invokes the provisioner *events* lambda function and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands events'

cluster-report: ## invokes the provisioner *report* lambda function and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands report --asjson'

cluster-version: ## calls the cluster version API and displays the response
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands version --asjson'

cluster-info: ## returns metadata associated with the user token
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands info --asjson'

cluster-test:
	VERSION=$(VERSION) \
	IS_PUBLIC=$(IS_PUBLIC) \
	DOMAIN=$(DOMAIN) \
	CLUSTER=$(CLUSTER) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	DESTROY_LAMBDA_ARN=$(DESTROY_LAMBDA_ARN) \
	SCHEDULE_LAMBDA_ARN=$(SCHEDULE_LAMBDA_ARN) \
	NODE_CAPACITY=$(NODE_CAPACITY) \
	TTL=$(TTL) \
	ALERT_STREAM=$(ALERT_STREAM) \
	TELEMETRY_STREAM=$(TELEMETRY_STREAM) \
	CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
	IPV4=$(IPV4) \
	ENVIRONMENT_VERSION=$(ENVIRONMENT_VERSION) \
	JWT_ISSUER=$(JWT_ISSUER) \
	AWS_REGION=$(AWS_REGION) \
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(ROOT)/applications/provisioner/provisioner.py $(TEST)'

cluster-deploy-candidate: ## deploy candidate public cluster
	@echo "Deploying cluster <sliderule-$(TIMESTAMP)>"
	conda run -n sliderule --no-capture-output \
	sh -c 'python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster sliderule-$(TIMESTAMP) --is_public true --node_capacity 7 --ttl 525600 --version latest --commands deploy'

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

cluster-direct-describe: ## directly status cluster created via cloud formation
	aws cloudformation describe-stack-events --stack-name $(CLUSTER)-cluster

cluster-direct-destroy: ## directly destroys cluster via cloud formation
	aws cloudformation delete-stack --stack-name $(CLUSTER)-cluster
	aws cloudformation wait stack-delete-complete --stack-name $(CLUSTER)-cluster

cluster-direct-resources: ## list resources associated with cluster that are present
	aws cloudformation describe-stack-resources --stack-name $(CLUSTER)-cluster \
		--query 'StackResources[?ResourceStatus!=`DELETE_COMPLETE`].[LogicalResourceId,ResourceStatus,ResourceType]' \
		--output table

########################
# Test Runner Targets
########################
#	- Requires the provisioner to be created
#	- The provisioner provides lambdas that are used to destroy the testrunner stack
#	- The testrunner automatically destroys itself after one hour

testrunner-create: ## invokes the provisioner lambda function and displays the response
	aws cloudformation create-stack \
		--stack-name 	$(TESTRUNNER_STACKNAME) \
		--template-body file://$(ROOT)/applications/provisioner/testrunner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=ContainerRegistry,ParameterValue=$(CONTAINER_REGISTRY) \
						ParameterKey=DestroyLambdaArn,ParameterValue=$(DESTROY_LAMBDA_ARN) \
						ParameterKey=ScheduleLambdaArn,ParameterValue=$(SCHEDULE_LAMBDA_ARN) \
						ParameterKey=DeployDate,ParameterValue=$(TIMESTAMP) \
						ParameterKey=Branch,ParameterValue=$(BRANCH)
	aws cloudformation wait stack-create-complete --stack-name $(TESTRUNNER_STACKNAME)

testrunner-describe: ## status the creation/deletion of the test runner
	aws cloudformation describe-stack-events --stack-name $(TESTRUNNER_STACKNAME)

testrunner-delete: ## deletes the test runner and any lingering event bridge rules (debug only)
	aws cloudformation delete-stack --stack-name $(TESTRUNNER_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(TESTRUNNER_STACKNAME)
	- aws events remove-targets --rule $(TESTRUNNER_STACKNAME)-auto-delete --ids 1
	- aws events delete-rule --name $(TESTRUNNER_STACKNAME)-auto-delete

testrunner-report: ## invokes the provisioner *report* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --commands report --report tests --asjson

testrunner-run: ## invokes the provisioner *report* lambda function and displays the response
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --domain $(DOMAIN) --cluster $(CLUSTER) --branch main --commands test

########################
# Recorder Targets
########################

recorder-create:  ## creates the lambda recorder functions
	aws cloudformation create-stack \
		--stack-name 	$(RECORDER_STACKNAME) \
		--template-body file://$(ROOT)/applications/recorder/recorder.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=AlertStream,ParameterValue=$(ALERT_STREAM) \
						ParameterKey=TelemetryStream,ParameterValue=$(TELEMETRY_STREAM)
	aws cloudformation wait stack-create-complete --stack-name $(RECORDER_STACKNAME)

recorder-describe: ## status the creation/deletion of the recorder functions
	aws cloudformation describe-stack-events --stack-name $(RECORDER_STACKNAME)

recorder-delete: ## deletes the lambda recorder functions
	aws athena delete-work-group --work-group $(RECORDER_STACKNAME)-workgroup --recursive-delete-option
	aws cloudformation delete-stack --stack-name $(RECORDER_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(RECORDER_STACKNAME)

########################
# Authenticator Targets
########################

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
	cd $(AUTHENTICATOR_STAGE_DIR)/package/ && zip -r -q $(AUTHENTICATOR_STAGE_DIR)/$(AUTHENTICATOR_ZIPFILE) . -x "/*__pycache__/*"

authenticator-create: authenticator-build  ## creates the lambda authenticator functions
	aws s3 cp $(AUTHENTICATOR_STAGE_DIR)/$(AUTHENTICATOR_ZIPFILE) s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(AUTHENTICATOR_ZIPFILE)
	aws cloudformation create-stack \
		--stack-name 	$(AUTHENTICATOR_STACKNAME) \
		--template-body file://$(ROOT)/applications/authenticator/authenticator.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=AuthenticatorHostname,ParameterValue=$(AUTHENTICATOR_HOSTNAME) \
						ParameterKey=GitHubOrg,ParameterValue=$(GITHUB_ORG) \
						ParameterKey=GitHubClientSecretName,ParameterValue=$(GITHUB_CLIENT_SECRET_NAME) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
						ParameterKey=CertificateArn,ParameterValue=$(CERTIFICATE_ARN) \
						ParameterKey=TrustedRedirectHosts,ParameterValue=$(TRUSTED_REDIRECT_HOSTS) \
						ParameterKey=ThirdPartyRedirectHosts,ParameterValue=$(THIRD_PARTY_REDIRECT_HOSTS) \
						ParameterKey=CorsAllowOrigins,ParameterValue=$(CORS_ALLOW_ORIGINS) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=AffiliatesFilename,ParameterValue=$(AFFILIATES_FILENAME) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/$(AUTHENTICATOR_ZIPFILE)
	aws cloudformation wait stack-create-complete --stack-name $(AUTHENTICATOR_STACKNAME)

authenticator-describe: ## status the creation/deletion of the authenticator functions
	aws cloudformation describe-stack-events --stack-name $(AUTHENTICATOR_STACKNAME)

authenticator-output: ## get the outputs from the creation of the authenticator stack
	aws cloudformation describe-stacks --stack-name $(AUTHENTICATOR_STACKNAME) --output text

authenticator-delete: ## deletes the lambda authenticator functions
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(AUTHENTICATOR_ZIPFILE)
	aws cloudformation delete-stack --stack-name $(AUTHENTICATOR_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(AUTHENTICATOR_STACKNAME)

authenticator-logs:
	aws logs tail /aws/lambda/$(AUTHENTICATOR_STACKNAME)-gateway --since 2h --format short

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
		-c "pip install --target /host/package -r /host/requirements.txt"
	cp $(ROOT)/applications/provisioner/provisioner.py $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/scheduler.py $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/cluster.yml $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/userasg.yml $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/testrunner.yml $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/ilb.sh $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/monitor.sh $(PROVISIONER_STAGE_DIR)/package/
	cp $(ROOT)/applications/provisioner/node.sh $(PROVISIONER_STAGE_DIR)/package/
	cd $(PROVISIONER_STAGE_DIR)/package/ && zip -r -q $(PROVISIONER_STAGE_DIR)/$(PROVISIONER_ZIPFILE) . -x "/*__pycache__/*"

provisioner-create: provisioner-build ## creates the lambda provisioner functions
	aws s3 cp $(PROVISIONER_STAGE_DIR)/$(PROVISIONER_ZIPFILE) s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(PROVISIONER_ZIPFILE)
	aws cloudformation create-stack \
		--stack-name 	$(PROVISIONER_STACKNAME) \
		--template-body file://$(ROOT)/applications/provisioner/provisioner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=ProvisionerHostname,ParameterValue=$(PROVISIONER_HOSTNAME) \
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
						ParameterKey=AffiliatesFilename,ParameterValue=$(AFFILIATES_FILENAME) \
						ParameterKey=SupportEmail,ParameterValue=$(SUPPORT_EMAIL) \
						ParameterKey=AlertEmail,ParameterValue=$(ALERT_EMAIL) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/$(PROVISIONER_ZIPFILE)
	aws cloudformation wait stack-create-complete --stack-name $(PROVISIONER_STACKNAME)

provisioner-describe: ## status the creation/deletion of the provisioner functions
	aws cloudformation describe-stack-events --stack-name $(PROVISIONER_STACKNAME)

provisioner-delete: ## deletes the lambda provisioner functions
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(PROVISIONER_ZIPFILE)
	aws cloudformation delete-stack --stack-name $(PROVISIONER_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(PROVISIONER_STACKNAME)

provisioner-logs:
	aws logs tail /aws/lambda/$(PROVISIONER_STACKNAME)-gateway --since 2h --format short

########################
# Runner Targets
########################

runner-build: ## create the runner lambda zipfile
	-rm -Rf $(RUNNER_STAGE_DIR)
	mkdir -p $(RUNNER_STAGE_DIR)
	cp $(ROOT)/applications/runner/requirements.txt $(RUNNER_STAGE_DIR)/
	docker run -it --rm \
		-v $(RUNNER_STAGE_DIR):/host \
		--user $(shell id -u):$(shell id -g) \
		--entrypoint /bin/bash \
		public.ecr.aws/lambda/python:3.13 \
		-c "pip install --target /host/package -r /host/requirements.txt"
	cp $(ROOT)/applications/runner/runner.py $(RUNNER_STAGE_DIR)/package/
	cd $(RUNNER_STAGE_DIR)/package/ && zip -r -q $(RUNNER_STAGE_DIR)/$(RUNNER_ZIPFILE) . -x "/*__pycache__/*"

runner-create: runner-build ## creates the lambda runner functions
	aws s3 cp $(RUNNER_STAGE_DIR)/$(RUNNER_ZIPFILE) s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(RUNNER_ZIPFILE)
	aws cloudformation create-stack \
		--stack-name 	$(RUNNER_STACKNAME) \
		--template-body file://$(ROOT)/applications/runner/runner.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters	ParameterKey=EnvironmentVersion,ParameterValue=$(ENVVER) \
						ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=RunnerHostname,ParameterValue=$(RUNNER_HOSTNAME) \
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
						ParameterKey=SupportEmail,ParameterValue=$(SUPPORT_EMAIL) \
						ParameterKey=AlertEmail,ParameterValue=$(ALERT_EMAIL) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/$(RUNNER_ZIPFILE)
	aws cloudformation wait stack-create-complete --stack-name $(RUNNER_STACKNAME)

runner-describe: ## status the creation/deletion of the runner functions
	aws cloudformation describe-stack-events --stack-name $(RUNNER_STACKNAME)

runner-delete: ## deletes the lambda runner functions
	- aws batch update-job-queue --job-queue $(RUNNER_STACKNAME)-job-queue --state DISABLED
	- python $(PYTHON_CLIENT_DIR)/utils/srcli.py --commands job_cancel_all
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(RUNNER_ZIPFILE)
	aws cloudformation delete-stack --stack-name $(RUNNER_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(RUNNER_STACKNAME)

runner-jobs-inprogress: ## displays all inprogress jobs
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --commands job_queue --args "SUBMITTED PENDING RUNNABLE STARTING RUNNING"

runner-jobs-finished: ## displays all finished jobs
	python $(PYTHON_CLIENT_DIR)/utils/srcli.py --commands job_queue --args "SUCCEEDED FAILED"

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
	cd $(CERTBOT_STAGE_DIR)/package/ && zip -r -q $(CERTBOT_STAGE_DIR)/$(CERTBOT_ZIPFILE) . -x "/*__pycache__/*"

certbot-create: certbot-build ## creates the lambda certbot function
	aws s3 cp $(CERTBOT_STAGE_DIR)/$(CERTBOT_ZIPFILE) s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(CERTBOT_ZIPFILE)
	aws cloudformation create-stack \
		--stack-name 	$(CERTBOT_STACKNAME) \
		--template-body file://$(ROOT)/applications/certbot/certbot.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		$(AWS_REGION) \
		--parameters 	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=ProjectBucket,ParameterValue=$(PROJECT_BUCKET) \
						ParameterKey=ProjectFolder,ParameterValue=$(PROJECT_FOLDER) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
						ParameterKey=LambdaZipFile,ParameterValue=$(PROJECT_FOLDER)/$(CERTBOT_ZIPFILE)
	aws cloudformation wait stack-create-complete --stack-name $(CERTBOT_STACKNAME)

certbot-describe: ## status the creation/deletion of the certbot function
	aws cloudformation describe-stack-events --stack-name $(CERTBOT_STACKNAME)

certbot-delete: ## deletes the lambda certbot function
	aws s3 rm s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(CERTBOT_ZIPFILE)
	aws cloudformation delete-stack --stack-name $(CERTBOT_STACKNAME)
	aws cloudformation wait stack-delete-complete --stack-name $(CERTBOT_STACKNAME)

########################
# Website Targets
########################

documentation: ## generate the static documentation (uses docs/environment.yml "rtd" conda environment)
	rm -Rf $(DOCUMENTATION_STAGE_DIR)
	mkdir -p $(DOCUMENTATION_STAGE_DIR)
	cp -R $(WEBSITE_SOURCE_DIR)/rtd $(DOCUMENTATION_STAGE_DIR)
	rsync -a $(ROOT) $(DOCUMENTATION_STAGE_DIR) --exclude build --exclude stage
	cp $(DOCUMENTATION_ASSET_DIR)/* $(DOCUMENTATION_STAGE_DIR)/rtd/source/assets
	conda run -n rtd --no-capture-output \
	sh -c 'pip install $(PYTHON_CLIENT_DIR) && \
	cd $(DOCUMENTATION_STAGE_DIR)/rtd && \
	sphinx-build -M html "source" "build"'

documentation-run: ## locally run the documentation
	conda run -n rtd --no-capture-output \
	sh -c 'cd $(DOCUMENTATION_STAGE_DIR)/rtd/build/html && \
	python -m http.server 8000'

documentation-live-update: ## update the documentation in the S3 bucket and invalidate the CloudFront cache
	aws s3 sync $(DOCUMENTATION_STAGE_DIR)/rtd/build/html s3://$(PROJECT_WEBSITE_BUCKET) --delete
	aws cloudfront create-invalidation --distribution-id $(DISTRIBUTION_ID) --paths "/*"

documentation-create: ## create documentation stack
	aws cloudformation create-stack \
		--stack-name 	documentation \
		--template-body file://$(ROOT)/docs/cloudfront/documentation.yml \
		--capabilities 	CAPABILITY_NAMED_IAM \
		--region 		us-east-1 \
		--parameters 	ParameterKey=Domain,ParameterValue=$(DOMAIN) \
						ParameterKey=DocumentationHostname,ParameterValue=$(DOCUMENTATION_HOSTNAME) \
						ParameterKey=HostedZoneId,ParameterValue=$(HOSTED_ZONE_ID) \
						ParameterKey=ProjectWebsiteBucket,ParameterValue=$(PROJECT_WEBSITE_BUCKET)
	aws cloudformation wait stack-create-complete --region us-east-1 --stack-name documentation

documentation-describe: ## status the creation/deletion of the documentation
	aws cloudformation describe-stack-events --region us-east-1 --stack-name documentation

documentation-delete: ## deletes the documentation stack
	aws cloudformation delete-stack --region us-east-1 --stack-name documentation
	aws cloudformation wait stack-delete-complete --region us-east-1 --stack-name documentation

documentation-render-notebooks: ## execute the notebooks used in the documentation
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/boulder_watershed.ipynb --output $(DOCUMENTATION_ASSET_DIR)/boulder_watershed.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/grandmesa.ipynb --output $(DOCUMENTATION_ASSET_DIR)/grandmesa.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/phoreal.ipynb --output $(DOCUMENTATION_ASSET_DIR)/phoreal.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/arcticdem_mosaic.ipynb --output $(DOCUMENTATION_ASSET_DIR)/arcticdem_mosaic.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/grandmesa_atl03_classification.ipynb --output $(DOCUMENTATION_ASSET_DIR)/grandmesa_atl03_classification.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/atl13_access.ipynb --output $(DOCUMENTATION_ASSET_DIR)/atl13_access.ipynb --debug
	$(DOCUMENTATION_CONDA_ACTIVATE) && jupyter nbconvert --to notebook --execute $(PYTHON_EXAMPLES_DIR)/atl24_access.ipynb --output $(DOCUMENTATION_ASSET_DIR)/atl24_access.ipynb --debug

########################
# Release Target
########################

release: ## tag and release sliderule; needs RELEASE
	make sliderule-buildenv-docker
	make ams-lock
	echo $(RELEASE) > $(ROOT)/version.txt
	cp $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-linux-${ARCH}.lock
	git add $(ROOT)/targets/slideruleearth/docker/ams/conda-lock.yml
	git add $(ROOT)/targets/slideruleearth/docker/sliderule/libdep-${ARCH}.lock
	git commit -m "Version $(RELEASE)"
	git tag -a $(RELEASE) -m "Version $(RELEASE)"
	git push --tags && git push
	gh release create $(RELEASE) -t $(RELEASE) --notes "see https://slideruleearth.io/developer_guide/release_notes/release_notes.html"
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

selftest: ## run the self test on the server code
	DOMAIN=localhost \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_REGION=$(AWS_REGION) \
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(TEST)

sliderule-test: ## run python client tests
	conda run -n ams --no-capture-output \
	sh -c 'cd $(ROOT)/applications/ams && coverage run -m pytest --domain $(DOMAIN) --organization $(CLUSTER) $(PYTEST) && coverage report -m'

ams-test: ## run ams tests
	conda run -n ams --no-capture-output \
	sh -c 'cd $(ROOT)/applications/ams && coverage run -m pytest $(PYTEST) && coverage report -m'

authenticator-test: ## run authenticator tests
	conda run -n authenticator --no-capture-output \
	sh -c 'cd $(ROOT)/applications/authenticator && \
	DOMAIN=localhost \
	AUTHENTICATOR_HOSTNAME=localhost \
	GITHUB_ORG=myorg \
	GITHUB_CLIENT_SECRET_NAME=$(DOMAIN)/github-oauth-client \
	HMAC_SIGNING_KEY_ARN=$(DOMAIN)/github-oauth-hmac-key \
	TRUSTED_REDIRECT_HOSTS=localhost \
	THIRD_PARTY_REDIRECT_HOSTS=companyx.com \
	SESSION_TABLE=$(AUTHENTICATOR_STACKNAME)-store \
	GITHUB_AUTHORIZE_URL=http://localhost:9083/login/oauth/authorize \
	GITHUB_TOKEN_URL=http://localhost:9083/login/oauth/access_token \
	GITHUB_DEVICE_CODE_URL=http://localhost:9083/login/device/code \
	GITHUB_API_URL=http://localhost:9083/api \
	JWT_SIGNING_KEY_ARN=$(JWT_SIGNING_KEY_ARN) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	AFFILIATES_FILENAME=$(AFFILIATES_FILENAME) \
	coverage run -m pytest $(PYTEST) && \
	coverage report -m'

provisioner-test: ## run provisioner tests
	conda run -n provisioner --no-capture-output \
	sh -c 'cd $(ROOT)/applications/provisioner && \
    DOMAIN=$(DOMAIN) \
	PROJECT_BUCKET=$(PROJECT_BUCKET) \
	PROJECT_FOLDER=$(PROJECT_FOLDER) \
	AFFILIATES_FILENAME=$(AFFILIATES_FILENAME) \
    SUPPORT_EMAIL=$(SUPPORT_EMAIL) \
	coverage run -m pytest $(PYTEST) && \
	coverage report -m'

nodejs-jest: ## run node.js client self tests
	make -C $(NODEJS_CLIENT_DIR) test DOMAIN=$(DOMAIN) ORGANIZATION=$(CLUSTER) TEST=$(JEST)

########################
# Help Targets
########################

update-cf-env: ## update cloud formation environment for public deployment
	aws s3 cp /data/$(AFFILIATES_FILENAME) s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/

init-dev-env: ## one-time setup of development environment for running locally
	mkdir -p /data
	aws s3 cp s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/ams/ /data/ --recursive
	mkdir -p /etc/haproxy/pem
	curl $(JWT_ISSUER)/auth/pem > /etc/haproxy/pem/pubkey.pem
	mkdir -p /etc/ssl/private
	aws s3 cp s3://$(PROJECT_BUCKET)/$(PROJECT_FOLDER)/$(DOMAIN).pem /etc/ssl/private/$(DOMAIN).pem

environment: ## echo the environment variables
	@echo INSTALLDIR: $(INSTALLDIR)
	@echo PROJECT_BUCKET: $(PROJECT_BUCKET)
	@echo PROJECT_FOLDER: $(PROJECT_FOLDER)
	@echo PROJECT_PUBLIC_BUCKET: $(PROJECT_PUBLIC_BUCKET)
	@echo PROJECT_WEBSITE_BUCKET: $(PROJECT_WEBSITE_BUCKET)
	@echo AWS_ACCOUNT: $(AWS_ACCOUNT)
	@echo AWS_REGION: $(AWS_REGION)
	@echo CONTAINER_REGISTRY: $(CONTAINER_REGISTRY)
	@echo IPV4: $(IPV4)
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
	@echo BASE_DOMAIN: $(BASE_DOMAIN)

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
