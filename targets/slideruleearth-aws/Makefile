# In order to complete the release process, the following must be installed and configured in your environment:
#     * GitHub command line client (gh)
#           See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#           The user must authenticate to GitHub via `gh auth login`
#     * AWS command line client (awscli)
#           See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#           The user must have up-to-date aws credentials
#     * Docker
#           The user must be logged into the AWS Elastic Container Registry (ECR)
#           The user must be logged into the GitHub Container Registry (GHCR)
#     * Terraform & Packer
#           The binaries are sufficient, but pay close attention to the local package versions
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
#     4. Login to NPMJS
#        $ cd sliderule/clients/nodejs && make login
#     5. Execute the makefile target to release the code; X.Y.Z is the version
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
MANAGER_STAGE_DIR = $(STAGE)/manager
TF_STAGE_DIR = $(STAGE)/tf
STATIC_WEB_SOURCE_DIR = $(ROOT)/docs
STATIC_WEB_STAGE_DIR = $(STAGE)/website
STATIC_WEB_EXPORT_DIR = $(STATIC_WEB_STAGE_DIR)/dist
PYTHON_EXAMPLES_DIR = $(ROOT)/../sliderule-python

INSTALLDIR ?= $(SLIDERULE_STAGE_DIR)

VERSION ?= latest
ECR ?= 742127912612.dkr.ecr.us-west-2.amazonaws.com
GHCR ?= ghcr.io/slideruleearth
DOCKEROPTS ?= # --progress=plain --no-cache
BUILDX ?= # buildx
DOCKER_PLATFORM ?= # --platform linux/arm64 | linux/amd64
ARCH ?= $(shell arch)
DOMAIN ?= slideruleearth.io
DOMAIN_ROOT = $(firstword $(subst ., ,$(DOMAIN)))
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER = $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
USERCFG ?= # e.g. -DSKIP_STATIC_ANALYSIS=1
DISTRIBUTION_ID = $(shell aws cloudfront list-distributions --query "DistributionList.Items[?Aliases.Items[0]=='$(DOMAIN)'].Id" --output text)
RUNNER ?= # e.g. valgrind

ORGANIZATION ?= sliderule
PUBLIC_IP ?= # output from public-cluster-deploy
COLOR ?=

CLANG_VER ?= ""
CLANG_CFG = export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER)

COMMON_CFG := -DINSTALLDIR=$(INSTALLDIR)
COMMON_CFG += -DUSE_HDF_PACKAGE=ON
COMMON_CFG += $(USERCFG)

DEBUG_CFG := -DCMAKE_BUILD_TYPE=Debug
DEBUG_CFG += -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
DEBUG_CFG += -DENABLE_ADDRESS_SANITIZER=ON
DEBUG_CFG += $(COMMON_CFG)

RELEASE_CFG := -DCMAKE_BUILD_TYPE=Release
RELEASE_CFG += $(COMMON_CFG)

########################
# Build Targets
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
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) IS_PUBLIC=False $(RUNNER) $(INSTALLDIR)/bin/sliderule $(ROOT)/targets/slideruleearth-aws/server.lua config.json

buildenv: ## run the build environment docker container
	docker run \
		-it \
		--network host \
		--user $(id -u):$(id -g) \
		-v $(shell realpath ~/.ssh):/root/.ssh \
		-v $(ROOT):$(ROOT) \
		-v /data:/data \
		-e MYIP=$(MYIP) \
		-e ROOT=$(ROOT) \
		-e GITNAME="$(shell git config user.name)" \
		-e GITEMAIL="$(shell git config user.email)" \
		--rm \
		--name buildenv \
		$(ECR)/sliderule-buildenv:$(VERSION)

buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cp docker/sliderule/.bashrc $(BUILDENV_STAGE_DIR)
	cd $(BUILDENV_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/sliderule-buildenv:$(VERSION) $(DOCKER_PLATFORM) .
	docker run -it -v ./docker/sliderule:/host --rm --name buildenv $(ECR)/sliderule-buildenv:$(VERSION) /bin/bash /host/libdep.sh /host/libdep-$(ARCH).lock
	docker tag $(ECR)/sliderule-buildenv:$(VERSION) $(GHCR)/sliderule-buildenv:$(VERSION)

buildenv-docker-push: ## push build environment docker image to registry
	docker push $(ECR)/sliderule-buildenv:$(VERSION)

buildenv-docker-push-github: ## push build environment docker image to GitHub registry
	docker push $(GHCR)/sliderule-buildenv:$(VERSION)

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SLIDERULE_STAGE_DIR)
	mkdir -p $(SLIDERULE_STAGE_DIR)
	rsync -a $(ROOT) $(SLIDERULE_STAGE_DIR) --exclude build --exclude stage
	cp docker/sliderule/Dockerfile.runtime $(SLIDERULE_STAGE_DIR)/Dockerfile
	cp docker/sliderule/config.json $(SLIDERULE_STAGE_DIR)
	cd $(SLIDERULE_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) --build-arg repo=$(ECR) -t $(ECR)/sliderule:$(VERSION) $(DOCKER_PLATFORM) .

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/intelligent-load-balancer/* $(ILB_STAGE_DIR)
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
	cp docker/monitor/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	chmod +x $(MONITOR_STAGE_DIR)/docker-entrypoint.sh
	cd $(MONITOR_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/monitor:$(VERSION) $(DOCKER_PLATFORM) .

manager-lock: ## update dependencies of manager
	cd docker/manager && conda-lock -p linux-$(ARCH) -f environment.yml
	cd docker/manager && conda-lock render -p linux-$(ARCH)

manager-docker: ## build manager docker image; needs VERSION
	-rm -Rf $(MANAGER_STAGE_DIR)
	mkdir -p $(MANAGER_STAGE_DIR)
	cp docker/manager/Dockerfile.$(ARCH) $(MANAGER_STAGE_DIR)/Dockerfile
	cp docker/manager/conda-* $(MANAGER_STAGE_DIR)
	cp docker/manager/pyproject.toml $(MANAGER_STAGE_DIR)
	cp docker/manager/docker-entrypoint.sh $(MANAGER_STAGE_DIR)
	cp $(ROOT)/version.txt $(MANAGER_STAGE_DIR)
	chmod +x $(MANAGER_STAGE_DIR)/docker-entrypoint.sh
	mkdir $(MANAGER_STAGE_DIR)/manager
	cp docker/manager/manager/*.py $(MANAGER_STAGE_DIR)/manager
	cp docker/manager/manager/*.sql $(MANAGER_STAGE_DIR)/manager
	cd $(MANAGER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/manager:$(VERSION) $(DOCKER_PLATFORM) .

manager-run:
	docker run -it -p 8000:8000 --rm --name manager $(ECR)/manager:$(VERSION) /bin/bash /docker-entrypoint.sh

########################
# Cluster Targets
########################

cluster-docker: ## build all cluster docker images
	make sliderule-docker
	make ilb-docker
	make proxy-docker
	make monitor-docker
	make manager-docker

cluster-docker-push: ## push all cluster images to docker container registries
	docker push $(ECR)/sliderule:$(VERSION)
	docker push $(ECR)/ilb:$(VERSION)
	docker push $(ECR)/proxy:$(VERSION)
	docker push $(ECR)/monitor:$(VERSION)
	docker push $(ECR)/manager:$(VERSION)

cluster-build-packer: ## build Amazon Machine Image (AMI) for release; needs VERSION
	cd packer; packer build -var version=$(VERSION) -var-file=arm64_cpu.json sliderule-base.pkr.hcl

cluster-upload-terraform: ## upload the Cluster terraform for provisioning system; needs VERSION
	-rm -Rf $(TF_STAGE_DIR)
	mkdir -p $(TF_STAGE_DIR)
	cp terraform/cluster/*.* $(TF_STAGE_DIR)
	cp terraform/cluster/.terraform.lock.hcl $(TF_STAGE_DIR)
	printf "variable \"environment_version\" {\n  default = \"$(ENVVER)\"\n}" > $(TF_STAGE_DIR)/version.tf
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(VERSION)

cluster: ## build all targets related to a cluster deployment and push to aws; needs VERSION
	make cluster-docker
	make cluster-docker-push
	make cluster-build-packer
	make cluster-upload-terraform

########################
# Deployment Targets
########################

public-cluster-test-color:
	@if [ -z "$(COLOR)" ]; then \
		echo "Error: COLOR must be set to green or blue"; \
		exit 1; \
	fi

public-cluster-test-ip:
	@if [ -z "$(PUBLIC_IP)" ]; then \
		echo "Error: COLOR must be set to green or blue"; \
		exit 1; \
	fi

public-cluster-status: public-cluster-test-color ## deploy public cluster using terraform; needs COLOR, uses DOMAIN
	cd terraform/cluster && terraform init
	cd terraform/cluster && terraform workspace select $(DOMAIN)-$(ORGANIZATION)-$(COLOR) || terraform workspace new $(DOMAIN)-$(ORGANIZATION)-$(COLOR)
	cd terraform/cluster && terraform plan -refresh-only && terraform show

public-cluster-deploy: public-cluster-test-color ## deploy public cluster using terraform; needs COLOR, VERSION, uses DOMAIN, ORGANIZATION, ENVVER
	cd terraform/cluster && terraform init
	cd terraform/cluster && terraform workspace select $(DOMAIN)-$(ORGANIZATION)-$(COLOR) || terraform workspace new $(DOMAIN)-$(ORGANIZATION)-$(COLOR)
	cd terraform/cluster && terraform apply -var is_public=True -var cluster_name=$(ORGANIZATION)-$(COLOR) -var domain=$(DOMAIN) -var cluster_version=$(VERSION) -var organization_name=$(ORGANIZATION) -var environment_version=$(ENVVER)

public-cluster-destroy: public-cluster-test-color ## destroy public using terraform; needs COLOR, uses DOMAIN
	cd terraform/cluster && terraform init
	cd terraform/cluster && terraform workspace select $(DOMAIN)-$(ORGANIZATION)-$(COLOR)
	cd terraform/cluster && terraform destroy

public-cluster-go-live: public-cluster-test-ip ## switch dns entry of public cluster; needs PUBLIC_IP, uses DOMAIN, ORGANIZATION
	cd terraform/dns && terraform init
	cd terraform/dns && terraform workspace select dns-$(DOMAIN)-$(ORGANIZATION) || terraform workspace new dns-$(DOMAIN)-$(ORGANIZATION)
	cd terraform/dns && terraform apply -var domain=$(DOMAIN) -var organization=$(ORGANIZATION) -var public_ip=$(PUBLIC_IP)

public-cluster-take-offline: ## destroy dns entry of public cluster; uses DOMAIN, ORGANIZATION
	cd terraform/dns && terraform init
	cd terraform/dns && terraform workspace select dns-$(DOMAIN)-$(ORGANIZATION)
	cd terraform/dns && terraform destroy

########################
# Static Website Targets
########################

static-website-docker: ## make the static website docker image; needs VERSION
	-rm -Rf $(STATIC_WEB_STAGE_DIR)
	mkdir -p $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/rtd $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/jekyll $(STATIC_WEB_STAGE_DIR)
	rsync -a $(ROOT) $(STATIC_WEB_STAGE_DIR) --exclude build --exclude stage
	-cp $(PYTHON_EXAMPLES_DIR)/examples/* $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp $(PYTHON_EXAMPLES_DIR)/environment.yml $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp -R /data/web $(STATIC_WEB_STAGE_DIR)/rtd/source/_static/html
	cp docker/static-website/Dockerfile.$(ARCH) $(STATIC_WEB_STAGE_DIR)/Dockerfile
	cp docker/static-website/nginx.conf $(STATIC_WEB_STAGE_DIR)
	cp docker/static-website/docker-entrypoint.sh $(STATIC_WEB_STAGE_DIR)
	cd $(STATIC_WEB_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/static-website:$(VERSION) $(DOCKER_PLATFORM) .

static-website-run: ## locally run the static website; needs VERSION
	docker run -it -p 4000:4000 --rm --name website $(ECR)/static-website:$(VERSION)

static-website-export: ## export static website to local host directory; needs VERSION
	-rm -Rf $(STATIC_WEB_EXPORT_DIR)
	mkdir -p $(STATIC_WEB_EXPORT_DIR)
	docker run -it --entrypoint /bin/bash  -v ./docker/static-website:/host -v $(STATIC_WEB_EXPORT_DIR):/output --rm --name website $(ECR)/static-website:$(VERSION) /host/export_dist.sh

static-website-live-update: static-website-export # Update the static web site docs in the S3 bucket and invalidate the CloudFront cache
	aws s3 sync $(STATIC_WEB_EXPORT_DIR)/jekyll/_site s3://$(DOMAIN) --delete
	aws cloudfront create-invalidation --distribution-id $(DISTRIBUTION_ID) --paths "/*"

static-website-deploy: ## deploy static-website using terraform; needs DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform apply -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

static-website-destroy: ## destroy static-website using terraform; needs DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform destroy -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

########################
# Certbot Targets
########################

certbot-package: ## create the lambda python package for the certbot
	cd terraform/certbot/python && ./package.sh

certbot-deploy: certbot-package ## deploy certbot using terraform; needs DOMAIN
	cd terraform/certbot && terraform init
	cd terraform/certbot && terraform workspace select $(DOMAIN)-certbot || terraform workspace new $(DOMAIN)-certbot
	cd terraform/certbot && terraform apply

certbot-destroy: certbot-package ## destroy certbot using terraform; needs DOMAIN
	cd terraform/certbot && terraform init
	cd terraform/certbot && terraform workspace select $(DOMAIN)-certbot|| terraform workspace new $(DOMAIN)-certbot
	cd terraform/certbot && terraform destroy

########################
# Tag & Release Targets
########################

tag: ## create version tag in this repository and release it on GitHub
	echo $(VERSION) > $(ROOT)/version.txt
	node $(ROOT)/clients/nodejs/utils/modpkg.js $(VERSION)
	git add $(ROOT)/clients/nodejs/sliderule/package.json
	cp $(ROOT)/version.txt $(ROOT)/clients/python/version.txt
	git add $(ROOT)/version.txt $(ROOT)/clients/python/version.txt
	git add $(ROOT)/targets/slideruleearth-aws/docker/manager/conda-linux-${ARCH}.lock
	git add $(ROOT)/targets/slideruleearth-aws/docker/manager/conda-lock.yml
	git add $(ROOT)/targets/slideruleearth-aws/docker/sliderule/libdep-${ARCH}.lock
	git commit -m "Version $(VERSION)"
	git tag -a $(VERSION) -m "Version $(VERSION)"
	git push --tags && git push
	gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/web/rtd/developer_guide/release_notes/release_notes.html"
	make -C $(ROOT)/clients/nodejs publish

release: ## release a version of sliderule; needs VERSION
	make buildenv-docker
	make buildenv-docker-push
	make manager-lock
	make tag
	make cluster-docker
	make cluster-docker-push
	make cluster-build-packer
	make cluster-upload-terraform
	make static-website-docker
	make static-website-live-update

########################
# Clean Up Targets
########################

docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(GHCR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule:$(VERSION)
	- docker image rm $(ECR)/ilb:$(VERSION)
	- docker image rm $(ECR)/proxy:$(VERSION)
	- docker image rm $(ECR)/monitor:$(VERSION)
	- docker image rm $(ECR)/manager:$(VERSION)
	- docker image rm $(ECR)/static-website:$(VERSION)
	- docker system prune -f

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)
	- cd $(ROOT)/docs && ./clean.sh
	- cd $(ROOT)/clients/python && ./clean.sh
	- cd $(ROOT)/targets/slideruleearth-aws/docker/manager && ./clean.sh
	- find $(ROOT) -name ".cookies" -exec rm {} \;
	- rm -f utilities/report.pdf
	- rm -f utilities/report.xml

########################
# Test Targets
########################

TEST ?= $(ROOT)/targets/slideruleearth-aws/test_runner.lua cloud
MOSAICS_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_perf.lua
STRIPS_PERFORMANCE_TEST ?=  $(ROOT)/datasets/pgc/systests/arcticdem_strips_perf.lua
SUBSET_PERFORMANCE_TEST ?=  $(ROOT)/targets/slideruleearth-aws/systests/subset_perf_test.lua

MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_serial_vs_batch_perf.lua
STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST  ?= $(ROOT)/datasets/pgc/systests/arcticdem_strips_serial_vs_batch_perf.lua

selftest: ## run the self test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(TEST)

perfmtest: ## run mosaics performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_PERFORMANCE_TEST)

perfstest: ## run strips performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(STRIPS_PERFORMANCE_TEST)

perfsubsetest: ## run subset performance test
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(SUBSET_PERFORMANCE_TEST)

perfmsbtest: ## run mosaics serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

perfssbtest: ## run strips serial and batch performance test comparing results
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(STRIPS_SERIAL_VS_BATCH_PERFORMANCE_TEST)

########################
# Help Target
########################

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)
	@echo DOMAIN: $(DOMAIN)
	@echo DOMAIN_ROOT: $(DOMAIN_ROOT)
	@echo DISTRIBUTION_ID: $(DISTRIBUTION_ID)
	@echo MYIP: $(MYIP)