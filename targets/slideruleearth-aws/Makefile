# Notes:
#
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
#        $ make release VERSION=vX.Y.Z DOCKEROPTS=--no-cache
#
# To update the build environment for GitHub actions
#     1. Login to GitHub Container Registry; {my_github_key} is a file storing an access key created for your account
#        $ cat {my_github_key} | docker login ghcr.io -u jpswinski --password-stdin
#     2. Build the docker build environment on x86 machine (x86 is IMPORTANT to match GitHub actions environment)
#        $ make sliderule-buildenv-docker
#     3. Push to GitHub container registry
#        $ docker push ghcr.io/slideruleearth/sliderule-buildenv:latest
#

ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
TARGET_BUILD_DIR = $(BUILD)/target
ICESAT2_BUILD_DIR = $(BUILD)/icesat2
PGC_BUILD_DIR = $(BUILD)/pgc
LANDSAT_BUILD_DIR = $(BUILD)/landsat
GEDI_BUILD_DIR = $(BUILD)/gedi
USGS3DEP_BUILD_DIR = $(BUILD)/usgs3dep
OPENDATA_BUILD_DIR = $(BUILD)/opendata
GEBCO_BUILD_DIR = $(BUILD)/gebco
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
STATIC_WEB_EXPORT_DIR = $(STATIC_WEB_STAGE_DIR)/dist
INSTALL_DIR ?= $(SERVER_STAGE_DIR)

OPENOCEANS_CONTAINER_STAGE_DIR = $(STAGE)/openoceans
OPENOCEANS_CONTAINER_DIR = $(ROOT)/plugins/icesat2/containers/openoceans
OPENOCEANS_CONTAINER_NAME ?= openoceans
BATHYWRITER_CONTAINER_STAGE_DIR = $(STAGE)/bathywriter
BATHYWRITER_CONTAINER_DIR = $(ROOT)/plugins/icesat2/containers/bathywriter
BATHYWRITER_CONTAINER_NAME ?= bathywriter
COASTNET_CONTAINER_SOURCE_DIR = $(ROOT)/../coastnet
COASTNET_CONTAINER_STAGE_DIR = $(STAGE)/coastnet
COASTNET_CONTAINER_DIR = $(ROOT)/plugins/icesat2/containers/coastnet
COASTNET_CONTAINER_NAME ?= coastnet

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
GHCR := ghcr.io/slideruleearth
DOCKEROPTS ?=
BUILDX ?= # buildx
DOCKER_PLATFORM ?= # linux/arm64
ARCH ?= $(shell arch)
DOMAIN ?= testsliderule.org
DOMAIN_ROOT = $(firstword $(subst ., ,$(DOMAIN)))
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER = $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
USERCFG ?=
DISTRIBUTION_ID = $(shell aws cloudfront list-distributions --query "DistributionList.Items[?Aliases.Items[0]=='$(DOMAIN)'].Id" --output text)

CLANG_VER ?= ""
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
CLANG_CFG = export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER)

DEBUG_CFG := -DCMAKE_BUILD_TYPE=Debug
DEBUG_CFG += -DINSTALLDIR=$(INSTALL_DIR)
DEBUG_CFG += -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
DEBUG_CFG += -DENABLE_ADDRESS_SANITIZER=ON
DEBUG_CFG += -DUSE_ARROW_PACKAGE=ON
DEBUG_CFG += -DUSE_AWS_PACKAGE=ON
DEBUG_CFG += -DUSE_CCSDS_PACKAGE=ON
DEBUG_CFG += -DUSE_CRE_PACKAGE=ON
DEBUG_CFG += -DUSE_GEO_PACKAGE=ON
DEBUG_CFG += -DUSE_H5_PACKAGE=ON
DEBUG_CFG += -DUSE_LEGACY_PACKAGE=ON
DEBUG_CFG += -DUSE_NETSVC_PACKAGE=ON
DEBUG_CFG += -DUSE_PISTACHE_PACKAGE=ON
DEBUG_CFG += $(USERCFG)

RELEASE_CFG := -DCMAKE_BUILD_TYPE=Release
RELEASE_CFG += -DINSTALLDIR=$(INSTALL_DIR)
RELEASE_CFG += -DUSE_ARROW_PACKAGE=ON
RELEASE_CFG += -DUSE_AWS_PACKAGE=ON
RELEASE_CFG += -DUSE_CRE_PACKAGE=ON
RELEASE_CFG += -DUSE_GEO_PACKAGE=ON
RELEASE_CFG += -DUSE_H5_PACKAGE=ON
RELEASE_CFG += -DUSE_NETSVC_PACKAGE=ON
RELEASE_CFG += -DUSE_CCSDS_PACKAGE=OFF
RELEASE_CFG += -DUSE_LEGACY_PACKAGE=OFF
RELEASE_CFG += -DUSE_PISTACHE_PACKAGE=OFF
RELEASE_CFG += $(USERCFG)

########################
# Build Targets
########################

all: sliderule

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)
	mkdir -p $(TARGET_BUILD_DIR)
	mkdir -p $(ICESAT2_BUILD_DIR)
	mkdir -p $(PGC_BUILD_DIR)
	mkdir -p $(LANDSAT_BUILD_DIR)
	mkdir -p $(GEDI_BUILD_DIR)
	mkdir -p $(USGS3DEP_BUILD_DIR)
	mkdir -p $(OPENDATA_BUILD_DIR)
	mkdir -p $(GEBCO_BUILD_DIR)
	mkdir -p $(SWOT_BUILD_DIR)

config-debug: prep ## configure the server for running locally with debug symbols, optimizations off, static analysis, and address sanitizer
	cd $(SLIDERULE_BUILD_DIR) && $(CLANG_CFG) && cmake $(DEBUG_CFG) -DMAX_FREE_STACK_SIZE=1 -DENABLE_TRACING=ON $(ROOT)
	cd $(TARGET_BUILD_DIR)    && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/targets/slideruleearth-aws
	cd $(ICESAT2_BUILD_DIR)   && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/icesat2
	cd $(PGC_BUILD_DIR)       && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/pgc
	cd $(LANDSAT_BUILD_DIR)   && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/landsat
	cd $(GEDI_BUILD_DIR)      && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/gedi
	cd $(USGS3DEP_BUILD_DIR)  && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/usgs3dep
	cd $(OPENDATA_BUILD_DIR)  && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/opendata
	cd $(GEBCO_BUILD_DIR)     && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/gebco
	cd $(SWOT_BUILD_DIR)      && $(CLANG_CFG) && cmake $(DEBUG_CFG) $(ROOT)/plugins/swot

config-release: prep ## configure server to run a release version locally
	cd $(SLIDERULE_BUILD_DIR) && cmake $(RELEASE_CFG) -DMAX_FREE_STACK_SIZE=1 $(ROOT)
	cd $(TARGET_BUILD_DIR)    && cmake $(RELEASE_CFG) $(ROOT)/targets/slideruleearth-aws
	cd $(ICESAT2_BUILD_DIR)   && cmake $(RELEASE_CFG) $(ROOT)/plugins/icesat2
	cd $(PGC_BUILD_DIR)       && cmake $(RELEASE_CFG) $(ROOT)/plugins/pgc
	cd $(LANDSAT_BUILD_DIR)   && cmake $(RELEASE_CFG) $(ROOT)/plugins/landsat
	cd $(GEDI_BUILD_DIR)      && cmake $(RELEASE_CFG) $(ROOT)/plugins/gedi
	cd $(USGS3DEP_BUILD_DIR)  && cmake $(RELEASE_CFG) $(ROOT)/plugins/usgs3dep
	cd $(OPENDATA_BUILD_DIR)  && cmake $(RELEASE_CFG) $(ROOT)/plugins/opendata
	cd $(GEBCO_BUILD_DIR)     && cmake $(RELEASE_CFG) $(ROOT)/plugins/gebco
	cd $(SWOT_BUILD_DIR)      && cmake $(RELEASE_CFG) $(ROOT)/plugins/swot

sliderule: ## build the server using the local configuration
	make -j8 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install
	make -j8 -C $(TARGET_BUILD_DIR)
	make -C $(TARGET_BUILD_DIR) install
	make -j8 -C $(ICESAT2_BUILD_DIR)
	make -C $(ICESAT2_BUILD_DIR) install
	make -j8 -C $(PGC_BUILD_DIR)
	make -C $(PGC_BUILD_DIR) install
	make -j8 -C $(LANDSAT_BUILD_DIR)
	make -C $(LANDSAT_BUILD_DIR) install
	make -j8 -C $(GEDI_BUILD_DIR)
	make -C $(GEDI_BUILD_DIR) install
	make -j8 -C $(USGS3DEP_BUILD_DIR)
	make -C $(USGS3DEP_BUILD_DIR) install
	make -j8 -C $(OPENDATA_BUILD_DIR)
	make -C $(OPENDATA_BUILD_DIR) install
	make -j8 -C $(GEBCO_BUILD_DIR)
	make -C $(GEBCO_BUILD_DIR) install
	make -j8 -C $(SWOT_BUILD_DIR)
	make -C $(SWOT_BUILD_DIR) install

run: ## run the server locally
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) IS_PUBLIC=False $(INSTALL_DIR)/bin/sliderule $(ROOT)/scripts/apps/server.lua config.json

run-buildenv: ## run the build environment docker container
	docker run -it -v $(ROOT):/host --rm --name buildenv $(ECR)/sliderule-buildenv:$(VERSION)

manager-package: ## create the lambda python package for the manager
	cd terraform/manager/python && ./package.sh

manager-deploy: ## deploy manager using terraform; needs DOMAIN
	cd terraform/manager && terraform init
	cd terraform/manager && terraform workspace select $(DOMAIN)-manager || terraform workspace new $(DOMAIN)-manager
	cd terraform/manager && terraform apply

manager-destroy: ## destroy manager using terraform; needs DOMAIN
	cd terraform/manager && terraform init
	cd terraform/manager && terraform workspace select $(DOMAIN)-manager|| terraform workspace new $(DOMAIN)-manager
	cd terraform/manager && terraform destroy

openoceans-container-lock: ## create the lock file for the openoceans container image
	cd $(OPENOCEANS_CONTAINER_DIR) && conda-lock -p linux-aarch64 -f environment.yml
	cd $(OPENOCEANS_CONTAINER_DIR) && conda-lock render -p linux-aarch64

openoceans-container-docker: ## create the copenoceans container image
	-rm -Rf $(OPENOCEANS_CONTAINER_STAGE_DIR)
	mkdir -p $(OPENOCEANS_CONTAINER_STAGE_DIR)
	cp $(OPENOCEANS_CONTAINER_DIR)/Dockerfile $(OPENOCEANS_CONTAINER_STAGE_DIR)
	cp $(OPENOCEANS_CONTAINER_DIR)/conda-* $(OPENOCEANS_CONTAINER_STAGE_DIR)
	cp $(OPENOCEANS_CONTAINER_DIR)/*.py $(OPENOCEANS_CONTAINER_STAGE_DIR)
	cd $(OPENOCEANS_CONTAINER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/$(OPENOCEANS_CONTAINER_NAME):latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/$(OPENOCEANS_CONTAINER_NAME):latest $(ECR)/$(OPENOCEANS_CONTAINER_NAME):$(VERSION)
	docker tag $(ECR)/$(OPENOCEANS_CONTAINER_NAME):latest $(ECR)/$(OPENOCEANS_CONTAINER_NAME):$(MAJOR_VERSION)
	
openoceans-container-test: ## run the openoceans container
	docker run -it --rm -v /data:/data --name $(OPENOCEANS_CONTAINER_NAME) $(ECR)/$(OPENOCEANS_CONTAINER_NAME):$(VERSION) /env/bin/python /usr/local/etc/oceaneyes.py /data/AppServer.2/bathy_beam_1.csv /data/AppServer.2/bathy_beam_1.json /data/AppServer.2/openoceans_1.csv

bathywriter-container-lock: ## create the lock file for the bathywriter container image
	cd $(BATHYWRITER_CONTAINER_DIR) && conda-lock -p linux-aarch64 -f environment.yml
	cd $(BATHYWRITER_CONTAINER_DIR) && conda-lock render -p linux-aarch64

bathywriter-container-docker: ## create the bathywriter container image
	-rm -Rf $(BATHYWRITER_CONTAINER_STAGE_DIR)
	mkdir -p $(BATHYWRITER_CONTAINER_STAGE_DIR)
	cp $(BATHYWRITER_CONTAINER_DIR)/Dockerfile $(BATHYWRITER_CONTAINER_STAGE_DIR)
	cp $(BATHYWRITER_CONTAINER_DIR)/conda-* $(BATHYWRITER_CONTAINER_STAGE_DIR)
	cp $(BATHYWRITER_CONTAINER_DIR)/*.py $(BATHYWRITER_CONTAINER_STAGE_DIR)
	cd $(BATHYWRITER_CONTAINER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/$(BATHYWRITER_CONTAINER_NAME):latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/$(BATHYWRITER_CONTAINER_NAME):latest $(ECR)/$(BATHYWRITER_CONTAINER_NAME):$(VERSION)
	docker tag $(ECR)/$(BATHYWRITER_CONTAINER_NAME):latest $(ECR)/$(BATHYWRITER_CONTAINER_NAME):$(MAJOR_VERSION)
	
bathywriter-container-test: ## run the bathywriter docker container
	cp $(BATHYWRITER_CONTAINER_DIR)/*.json /data
	docker run -it --rm -v /data:/data --name $(BATHYWRITER_CONTAINER_NAME) $(ECR)/$(BATHYWRITER_CONTAINER_NAME):$(VERSION) /env/bin/python /usr/local/etc/writer.py data/test.json

coastnet-container-docker: ## create the coastnet container image
	-rm -Rf $(COASTNET_CONTAINER_STAGE_DIR)
	mkdir -p $(COASTNET_CONTAINER_STAGE_DIR)
	rsync -a $(COASTNET_CONTAINER_SOURCE_DIR) $(COASTNET_CONTAINER_STAGE_DIR)/coastnet --exclude build --exclude .git
	cp $(COASTNET_CONTAINER_DIR)/Dockerfile $(COASTNET_CONTAINER_STAGE_DIR)
	cp $(COASTNET_CONTAINER_DIR)/*.sh $(COASTNET_CONTAINER_STAGE_DIR)
	cp $(COASTNET_CONTAINER_DIR)/*.py $(COASTNET_CONTAINER_STAGE_DIR)
	cp $(COASTNET_CONTAINER_DIR)/CMakeLists.txt $(COASTNET_CONTAINER_STAGE_DIR)
	cp $(COASTNET_CONTAINER_DIR)/Makefile $(COASTNET_CONTAINER_STAGE_DIR)
	cd $(COASTNET_CONTAINER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/$(COASTNET_CONTAINER_NAME):latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/$(COASTNET_CONTAINER_NAME):latest $(ECR)/$(COASTNET_CONTAINER_NAME):$(VERSION)
	docker tag $(ECR)/$(COASTNET_CONTAINER_NAME):latest $(ECR)/$(COASTNET_CONTAINER_NAME):$(MAJOR_VERSION)
	
coastnet-container-surface-test: ## run the coastnet surface docker container
	docker run -it --rm -v /data:/data --name $(COASTNET_CONTAINER_NAME) $(ECR)/$(COASTNET_CONTAINER_NAME):$(VERSION) bash surface.sh nil nil /data/AppServer.2/bathy_spot_3.csv /data/AppServer.2/coastnet.csv

coastnet-container-bathy-test: ## run the coastnet bathy docker container
	docker run -it --rm -v /data:/data --name $(COASTNET_CONTAINER_NAME) $(ECR)/$(COASTNET_CONTAINER_NAME):$(VERSION) bash bathy.sh nil nil /data/AppServer.2/bathy_spot_3.csv /data/AppServer.2/coastnet.csv

static-website-docker: ## make the static website docker image; needs VERSION
	-rm -Rf $(STATIC_WEB_STAGE_DIR)
	mkdir -p $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/rtd $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/jekyll $(STATIC_WEB_STAGE_DIR)
	rsync -a $(ROOT) $(STATIC_WEB_STAGE_DIR)/sliderule --exclude build --exclude stage
	-cp $(PYTHON_EXAMPLES_DIR)/examples/* $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp $(PYTHON_EXAMPLES_DIR)/environment.yml $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp -R /data/web $(STATIC_WEB_STAGE_DIR)/rtd/source/_static/html
	cp docker/static-website/Dockerfile.$(ARCH) $(STATIC_WEB_STAGE_DIR)/Dockerfile
	cp docker/static-website/nginx.conf $(STATIC_WEB_STAGE_DIR)
	cp docker/static-website/docker-entrypoint.sh $(STATIC_WEB_STAGE_DIR)
	cd $(STATIC_WEB_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/static-website:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/static-website:latest $(ECR)/static-website:$(VERSION)
	docker tag $(ECR)/static-website:latest $(ECR)/static-website:$(MAJOR_VERSION)

static-website-run: ## locally run the static website; needs VERSION
	docker run -it -p 8080:4000 --rm --name website $(ECR)/static-website:$(VERSION)

static-website-export: ## export static website to local host directory
	-rm -Rf $(STATIC_WEB_EXPORT_DIR)
	mkdir -p $(STATIC_WEB_EXPORT_DIR)
	docker run -it --entrypoint /bin/bash  -v ./docker/static-website:/host -v $(STATIC_WEB_EXPORT_DIR):/output --rm --name website $(ECR)/static-website:$(VERSION) /host/export_dist.sh

static-website-live-update: # Update the static web site docs in the S3 bucket and invalidate the CloudFront cache
	aws s3 sync $(STATIC_WEB_EXPORT_DIR)/jekyll/_site s3://$(DOMAIN) --delete
	aws cloudfront create-invalidation --distribution-id $(DISTRIBUTION_ID) --paths "/*"

static-website-deploy: ## deploy static-website using terraform; needs VERSION, DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform apply -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

static-website-destroy: ## destroy static-website using terraform; needs DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform destroy -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

static-website-docker-push: ## push static-website docker image to registry
	docker push $(ECR)/static-website:$(VERSION)
	docker push $(ECR)/static-website:$(MAJOR_VERSION)

sliderule-buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cd $(BUILDENV_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/sliderule-buildenv:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/sliderule-buildenv:latest $(ECR)/sliderule-buildenv:$(VERSION)
	docker tag $(ECR)/sliderule-buildenv:latest $(ECR)/sliderule-buildenv:$(MAJOR_VERSION)
	docker tag $(ECR)/sliderule-buildenv:latest $(GHCR)/sliderule-buildenv:$(VERSION)
	docker run -it -v ./docker/sliderule:/host --rm --name buildenv $(ECR)/sliderule-buildenv:$(VERSION) /bin/bash /host/libdep.sh

sliderule-buildenv-docker-push: ## push build environment docker image to registry
	docker push $(ECR)/sliderule-buildenv:$(VERSION)
	docker push $(ECR)/sliderule-buildenv:$(MAJOR_VERSION)

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SERVER_STAGE_DIR)
	mkdir -p $(SERVER_STAGE_DIR)
	rsync -a $(ROOT) $(SERVER_STAGE_DIR)/sliderule --exclude build --exclude stage
	cp docker/sliderule/Dockerfile.runtime $(SERVER_STAGE_DIR)/Dockerfile
	cp docker/sliderule/config.json $(SERVER_STAGE_DIR)
	cd $(SERVER_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) --build-arg repo=$(ECR) -t $(ECR)/sliderule:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(VERSION)
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(MAJOR_VERSION)

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/intelligent-load-balancer/* $(ILB_STAGE_DIR)
	cp $(ROOT)/targets/orchestrator-haproxy/orchestrator.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/scripts/extensions/json.lua $(ILB_STAGE_DIR)
	cp $(ROOT)/scripts/extensions/prettyprint.lua $(ILB_STAGE_DIR)
	cd $(ILB_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/ilb:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/ilb:latest $(ECR)/ilb:$(VERSION)
	docker tag $(ECR)/ilb:latest $(ECR)/ilb:$(MAJOR_VERSION)

proxy-docker: # make the reverse proxy docker image; needs VERSION
	-rm -Rf $(PROXY_STAGE_DIR)
	mkdir -p $(PROXY_STAGE_DIR)
	cp docker/proxy/* $(PROXY_STAGE_DIR)
	cd $(PROXY_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/proxy:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/proxy:latest $(ECR)/proxy:$(VERSION)
	docker tag $(ECR)/proxy:latest $(ECR)/proxy:$(MAJOR_VERSION)

monitor-docker: ## build monitor docker image; needs VERSION
	-rm -Rf $(MONITOR_STAGE_DIR)
	mkdir -p $(MONITOR_STAGE_DIR)
	cp docker/monitor/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	chmod +x $(MONITOR_STAGE_DIR)/docker-entrypoint.sh
	cd $(MONITOR_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/monitor:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/monitor:latest $(ECR)/monitor:$(VERSION)
	docker tag $(ECR)/monitor:latest $(ECR)/monitor:$(MAJOR_VERSION)

cluster-docker: ## build all cluster docker images
	make sliderule-docker
	make ilb-docker
	make proxy-docker
	make monitor-docker

cluster-docker-push: ## push all cluster images to docker container registries
	docker push $(ECR)/sliderule:$(VERSION)
	docker push $(ECR)/sliderule:$(MAJOR_VERSION)
	docker push $(ECR)/ilb:$(VERSION)
	docker push $(ECR)/ilb:$(MAJOR_VERSION)
	docker push $(ECR)/proxy:$(VERSION)
	docker push $(ECR)/proxy:$(MAJOR_VERSION)
	docker push $(ECR)/monitor:$(VERSION)
	docker push $(ECR)/monitor:$(MAJOR_VERSION)

cluster-build-packer: ## build Amazon Machine Image (AMI) for release; needs VERSION
	cd packer; packer build -var version=$(MAJOR_VERSION) sliderule-base.pkr.hcl

cluster-upload-terraform: ## upload the Cluster terraform for provisioning system; needs VERSION
	-rm -Rf $(TF_STAGE_DIR)
	mkdir -p $(TF_STAGE_DIR)
	cp terraform/cluster/*.* $(TF_STAGE_DIR)
	cp terraform/cluster/.terraform.lock.hcl $(TF_STAGE_DIR)
	printf "variable \"environment_version\" {\n  default = \"$(ENVVER)\"\n}" > $(TF_STAGE_DIR)/version.tf
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(VERSION)
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(MAJOR_VERSION)

tag: ## create version tag in this repository and release it on GitHub
	echo $(VERSION) > $(ROOT)/version.txt
	node $(ROOT)/clients/nodejs/utils/modpkg.js $(VERSION)
	git add $(ROOT)/clients/nodejs/sliderule/package.json
	cp $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git add $(ROOT)/version.txt $(PYTHON_CLIENT_DIR)/version.txt
	git commit -m "Version $(VERSION)"
	git tag -a $(VERSION) -m "Version $(VERSION)"
	git push --tags && git push
	gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/web/rtd/release_notes/release_notes.html"
	make -C $(ROOT)/clients/nodejs publish

release: ## release a version of sliderule; needs VERSION
	make tag
	make sliderule-buildenv-docker
	make sliderule-buildenv-docker-push
	make cluster-docker
	make cluster-docker-push
	make static-website-docker
	make static-website-docker-push
	make cluster-build-packer
	make cluster-upload-terraform

docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(GHCR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule-buildenv:$(VERSION)
	- docker image rm $(ECR)/sliderule:$(VERSION)
	- docker image rm $(ECR)/ilb:$(VERSION)
	- docker image rm $(ECR)/proxy:$(VERSION)
	- docker image rm $(ECR)/monitor:$(VERSION)
	- docker image rm $(ECR)/static-website:$(VERSION)
	- docker system prune -f

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)
	- cd $(ROOT)/docs && ./clean.sh
	- cd $(ROOT)/clients/python && ./clean.sh
	- find $(ROOT) -name ".cookies" -exec rm {} \;
	- rm -f report.pdf
	- rm -f report.xml

########################
# Test Targets
########################

TEST ?= $(ROOT)/scripts/selftests/test_runner.lua cloud
MOSAICS_PERFORMANCE_TEST ?= $(ROOT)/plugins/pgc/systests/arcticdem_mosaic_perf.lua
STRIPS_PERFORMANCE_TEST ?=  $(ROOT)/plugins/pgc/systests/arcticdem_strips_perf.lua
SUBSET_PERFORMANCE_TEST ?=  $(ROOT)/scripts/systests/subset_perf_test.lua

selftest: ## run the self test on the server code
	$(SERVER_STAGE_DIR)/bin/sliderule $(TEST)

perfmtest: ## run mosaics performance test on the server code
	$(SERVER_STAGE_DIR)/bin/sliderule $(MOSAICS_PERFORMANCE_TEST)

perfstest: ## run strips performance test on the server code
	$(SERVER_STAGE_DIR)/bin/sliderule $(STRIPS_PERFORMANCE_TEST)

perfsubsetest: ## run subset performance test
	$(SERVER_STAGE_DIR)/bin/sliderule $(SUBSET_PERFORMANCE_TEST)

########################
# Help Target
########################

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)
	@echo DOMAIN: $(DOMAIN)
	@echo DOMAIN_ROOT: $(DOMAIN_ROOT)
	@echo DISTRIBUTION_ID: $(DISTRIBUTION_ID)