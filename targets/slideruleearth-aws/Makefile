# Notes:
#
# (1) In order to complete the release process, the following must be installed and configured in your environment:
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
#
# (2) To release a version of SlideRule:
#     1. Update .aws/credentials file with a temporary session token; {profile} references your long term aws credentials, {username} is your aws account, {code} is your MFA token
#        $ aws --profile={profile} sts get-session-token --serial-number arn:aws:iam::742127912612:mfa/{username} --token-code={code}
#     2. Login to AWS Elastic Container Registry
#        $ aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
#     3. Login to GitHub Container Registry; {my_github_key} is a file storing an access key created for your account
#        $ cat {my_github_key} | docker login ghcr.io -u jpswinski --password-stdin
#     4. Login to GitHub
#        $ gh auth login
#     5. Execute the makefile target to release the code; X.Y.Z is the version
#        $ make cluster-release VERSION=vX.Y.Z DOCKEROPTS=--no-cache
#

ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_SOURCE_DIR = $(ROOT)
SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
ICESAT2_BUILD_DIR = $(BUILD)/icesat2
PGC_BUILD_DIR = $(BUILD)/pgc
LANDSAT_BUILD_DIR = $(BUILD)/landsat
GEDI_BUILD_DIR = $(BUILD)/gedi
USGS3DEP_BUILD_DIR = $(BUILD)/usgs3dep
SWOT_BUILD_DIR = $(BUILD)/swot
PYTHON_EXAMPLES_DIR = $(ROOT)/../sliderule-python
PYTHON_CLIENT_DIR = $(ROOT)/clients/python
NODEJS_CLIENT_DIR = $(ROOT)/clients/nodejs
SERVER_STAGE_DIR = $(STAGE)/sliderule
BUILDENV_STAGE_DIR = $(STAGE)/buildenv
MONITOR_STAGE_DIR = $(STAGE)/monitor
ILB_STAGE_DIR = $(STAGE)/ilb
PROXY_STAGE_DIR = $(STAGE)/proxy
TF_STAGE_DIR = $(STAGE)/tf
STATIC_WEB_SOURCE_DIR = $(ROOT)/docs
STATIC_WEB_STAGE_DIR = $(STAGE)/website

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
GHCR := ghcr.io/icesat2-sliderule
DOCKEROPTS ?=
ARCH ?= arm
DOMAIN ?= testsliderule.org
DOMAIN_ROOT = $(firstword $(subst ., ,$(DOMAIN)))
CLANG_VER = "-12"
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(SLIDERULE_SOURCE_DIR)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER = $(shell git --git-dir ../../.git --work-tree ../../.git describe --abbrev --dirty --always --tags --long)
TEST ?= $(SLIDERULE_SOURCE_DIR)/scripts/selftests/test_runner.lua
SLIDERULECFG := -DMAX_FREE_STACK_SIZE=1
SLIDERULECFG += -DUSE_ARROW_PACKAGE=ON
SLIDERULECFG += -DUSE_AWS_PACKAGE=ON
SLIDERULECFG += -DUSE_H5_PACKAGE=ON
SLIDERULECFG += -DUSE_NETSVC_PACKAGE=ON
SLIDERULECFG += -DUSE_GEO_PACKAGE=ON
SLIDERULECFG += -DUSE_LEGACY_PACKAGE=OFF
SLIDERULECFG += -DUSE_CCSDS_PACKAGE=OFF

all: sliderule

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)
	mkdir -p $(ICESAT2_BUILD_DIR)
	mkdir -p $(PGC_BUILD_DIR)
	mkdir -p $(LANDSAT_BUILD_DIR)
	mkdir -p $(GEDI_BUILD_DIR)
	mkdir -p $(USGS3DEP_BUILD_DIR)
	mkdir -p $(SWOT_BUILD_DIR)

config-debug: prep ## configure the server for running locally with debug symbols
	cd $(SLIDERULE_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug $(SLIDERULECFG) -DENABLE_TRACING=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(LANDSAT_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/landsat
	cd $(GEDI_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi
	cd $(USGS3DEP_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/usgs3dep
	cd $(SWOT_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/swot

config-release: prep ## configure server to run release version locally (useful for using valgrind)
	cd $(SLIDERULE_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release $(SLIDERULECFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(LANDSAT_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/landsat
	cd $(GEDI_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi
	cd $(USGS3DEP_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/usgs3dep
	cd $(SWOT_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/swot

config-asan: prep ## configure server to run with address sanitizer locally
	cd $(SLIDERULE_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON $(SLIDERULECFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(LANDSAT_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/landsat
	cd $(GEDI_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi
	cd $(USGS3DEP_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/usgs3dep
	cd $(SWOT_BUILD_DIR) && export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/swot

sliderule: ## build the server using the local configuration
	make -j4 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install
	make -j4 -C $(ICESAT2_BUILD_DIR)
	make -C $(ICESAT2_BUILD_DIR) install
	make -j4 -C $(PGC_BUILD_DIR)
	make -C $(PGC_BUILD_DIR) install
	make -j4 -C $(LANDSAT_BUILD_DIR)
	make -C $(LANDSAT_BUILD_DIR) install
	make -j4 -C $(GEDI_BUILD_DIR)
	make -C $(GEDI_BUILD_DIR) install
	make -j4 -C $(USGS3DEP_BUILD_DIR)
	make -C $(USGS3DEP_BUILD_DIR) install
	make -j4 -C $(SWOT_BUILD_DIR)
	make -C $(SWOT_BUILD_DIR) install

run: ## run the server locally
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) $(SERVER_STAGE_DIR)/bin/sliderule $(SLIDERULE_SOURCE_DIR)/scripts/apps/server.lua config.json

selftest: ## run the self test on the server code
	$(SERVER_STAGE_DIR)/bin/sliderule $(TEST)

static-website-docker: ## make the static website docker image; needs VERSION
	-rm -Rf $(STATIC_WEB_STAGE_DIR)
	mkdir -p $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/rtd $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/jekyll $(STATIC_WEB_STAGE_DIR)
	rsync -a $(SLIDERULE_SOURCE_DIR) $(STATIC_WEB_STAGE_DIR)/sliderule --exclude build --exclude stage
	-cp $(PYTHON_EXAMPLES_DIR)/examples/* $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp $(PYTHON_EXAMPLES_DIR)/environment.yml $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp -R /data/web $(STATIC_WEB_STAGE_DIR)/rtd/source/_static/html
	cp docker/static-website/Dockerfile.$(ARCH) $(STATIC_WEB_STAGE_DIR)/Dockerfile
	cp docker/static-website/nginx.conf $(STATIC_WEB_STAGE_DIR)
	cp docker/static-website/docker-entrypoint.sh $(STATIC_WEB_STAGE_DIR)
	cd $(STATIC_WEB_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(ECR)/static-website:latest .
	docker tag $(ECR)/static-website:latest $(ECR)/static-website:$(VERSION)
	docker tag $(ECR)/static-website:latest $(ECR)/static-website:$(MAJOR_VERSION)

static-website-deploy: ## deploy static-website using terraform; needs VERSION, DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform apply -var docker_image_url_static-website=$(ECR)/static-website:$(VERSION) -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

static-website-destroy: ## destroy static-website using terraform; needs DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform destroy -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

sliderule-buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cd $(BUILDENV_STAGE_DIR); docker build $(DOCKEROPTS) -t $(GHCR)/sliderule-buildenv:latest .
	docker tag $(GHCR)/sliderule-buildenv:latest $(GHCR)/sliderule-buildenv:$(VERSION)
	docker tag $(GHCR)/sliderule-buildenv:latest $(GHCR)/sliderule-buildenv:$(MAJOR_VERSION)

sliderule-buildenv-docker-push: ## push sliderule build environment docker image (must be x86 arch)
	docker push $(GHCR)/sliderule-buildenv:$(VERSION)
	docker push $(GHCR)/sliderule-buildenv:$(MAJOR_VERSION)

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SERVER_STAGE_DIR)
	mkdir -p $(SERVER_STAGE_DIR)/etc
	rsync -a $(SLIDERULE_SOURCE_DIR) $(SERVER_STAGE_DIR)/sliderule --exclude build --exclude stage
	cp docker/sliderule/Dockerfile.runtime $(SERVER_STAGE_DIR)/Dockerfile
	cp docker/sliderule/*.* $(SERVER_STAGE_DIR)/etc
	cd $(SERVER_STAGE_DIR); docker build $(DOCKEROPTS) --build-arg repo=$(GHCR) -t $(ECR)/sliderule:latest .
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(VERSION)
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(MAJOR_VERSION)

sliderule-full-docker: sliderule-buildenv-docker sliderule-docker ## build sliderule docker container from scratch; needs VERSION

monitor-docker: ## build monitor docker image; needs VERSION
	-rm -Rf $(MONITOR_STAGE_DIR)
	mkdir -p $(MONITOR_STAGE_DIR)
	cp docker/monitor/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	chmod +x $(MONITOR_STAGE_DIR)/docker-entrypoint.sh
	cd $(MONITOR_STAGE_DIR); docker build $(DOCKEROPTS) -t $(ECR)/monitor:latest .
	docker tag $(ECR)/monitor:latest $(ECR)/monitor:$(VERSION)
	docker tag $(ECR)/monitor:latest $(ECR)/monitor:$(MAJOR_VERSION)

proxy-docker: # make the reverse proxy docker image; needs VERSION
	-rm -Rf $(PROXY_STAGE_DIR)
	mkdir -p $(PROXY_STAGE_DIR)
	cp docker/proxy/* $(PROXY_STAGE_DIR)
	cd $(PROXY_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(ECR)/proxy:latest .
	docker tag $(ECR)/proxy:latest $(ECR)/proxy:$(VERSION)
	docker tag $(ECR)/proxy:latest $(ECR)/proxy:$(MAJOR_VERSION)

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/intelligent-load-balancer/* $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/targets/orchestrator-haproxy/orchestrator.lua $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/scripts/extensions/json.lua $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/scripts/extensions/prettyprint.lua $(ILB_STAGE_DIR)
	cd $(ILB_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(ECR)/ilb:latest .
	docker tag $(ECR)/ilb:latest $(ECR)/ilb:$(VERSION)
	docker tag $(ECR)/ilb:latest $(ECR)/ilb:$(MAJOR_VERSION)

cluster-docker: ## build all cluster docker images
	make sliderule-buildenv-docker
	make sliderule-docker
	make proxy-docker
	make monitor-docker
	make ilb-docker
	make static-website-docker

cluster-docker-push: ## push all cluster images to docker container registries
	docker push $(ECR)/sliderule:$(VERSION)
	docker push $(ECR)/sliderule:$(MAJOR_VERSION)
	docker push $(ECR)/proxy:$(VERSION)
	docker push $(ECR)/proxy:$(MAJOR_VERSION)
	docker push $(ECR)/monitor:$(VERSION)
	docker push $(ECR)/monitor:$(MAJOR_VERSION)
	docker push $(ECR)/ilb:$(VERSION)
	docker push $(ECR)/ilb:$(MAJOR_VERSION)
	docker push $(ECR)/static-website:$(VERSION)
	docker push $(ECR)/static-website:$(MAJOR_VERSION)

cluster-build-packer: ## build Amazon Machine Image (AMI) for release; needs VERSION
	cd packer; packer build -var version=$(MAJOR_VERSION) sliderule-base.pkr.hcl

cluster-upload-terraform: ## upload the Cluster terraform for provisioning system; needs VERSION
	-rm -Rf $(TF_STAGE_DIR)
	mkdir -p $(TF_STAGE_DIR)
	cp terraform/cluster/*.* $(TF_STAGE_DIR)
	cp terraform/cluster/.terraform.lock.hcl $(TF_STAGE_DIR)
	echo "variable \"environment_version\" {\n  default = \"$(ENVVER)\"\n}" > $(TF_STAGE_DIR)/version.tf
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(VERSION)
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(MAJOR_VERSION)

cluster-tag: ## create version tag in this repository and release it on GitHub
	echo $(VERSION) > $(ROOT)/version.txt
	node $(ROOT)/clients/nodejs/utils/modpkg.js $(VERSION)
	git add $(ROOT)/clients/nodejs/sliderule/package.json
	cp $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git commit -m "Version $(VERSION)"
	git tag -a $(VERSION) -m "Version $(VERSION)"
	git push --tags && git push
	gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/rtd/release_notes/release_notes.html"
	make -C $(ROOT)/clients/nodejs publish

cluster-release: ## release a version of the cluster; needs VERSION
	make cluster-tag
	make cluster-docker
	make cluster-docker-push
	make cluster-build-packer
	make cluster-upload-terraform

cluster-docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(GHCR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule:$(VERSION)
	- docker image rm $(ECR)/ilb:$(VERSION)
	- docker image rm $(ECR)/proxy:$(VERSION)
	- docker image rm $(ECR)/monitor:$(VERSION)
	- docker image rm $(ECR)/static-website:$(VERSION)
	- docker system prune -f

distclean: ## fully remove all non-version controlled files and directories
	make -C $(SLIDERULE_SOURCE_DIR) distclean

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)
