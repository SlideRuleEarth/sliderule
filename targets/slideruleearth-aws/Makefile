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
#        $ make sliderule-buildenv-docker
#     3. Push to GitHub container registry
#        $ docker push ghcr.io/slideruleearth/sliderule-buildenv:latest
#

ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
PYTHON_EXAMPLES_DIR = $(ROOT)/../sliderule-python
PYTHON_CLIENT_DIR = $(ROOT)/clients/python
NODEJS_CLIENT_DIR = $(ROOT)/clients/nodejs
SLIDERULE_STAGE_DIR = $(STAGE)/sliderule
BUILDENV_STAGE_DIR = $(STAGE)/buildenv
MONITOR_STAGE_DIR = $(STAGE)/monitor
ILB_STAGE_DIR = $(STAGE)/ilb
PROXY_STAGE_DIR = $(STAGE)/proxy
TF_STAGE_DIR = $(STAGE)/tf
STATIC_WEB_SOURCE_DIR = $(ROOT)/docs
STATIC_WEB_STAGE_DIR = $(STAGE)/website
STATIC_WEB_EXPORT_DIR = $(STATIC_WEB_STAGE_DIR)/dist
OCEANEYES_SRC_DIR = $(ROOT)/datasets/bathy/docker/oceaneyes
OCEANEYES_STAGE_DIR = $(STAGE)/oceaneyes
MAGIC_PYTORCH_DIR = $(ROOT)/../../miniconda3/envs/pointnet2
CSHELPH_SOURCE_DIR ?= $(ROOT)/../ut-ATL24-C-shelph
CSHELPH_SOURCE_FILE = $(CSHELPH_SOURCE_DIR)/c_shelph.py
CSHELPH_VERSION = $(shell git --work-tree ${CSHELPH_SOURCE_DIR} --git-dir ${CSHELPH_SOURCE_DIR}/.git describe --abbrev --dirty --always --tags --long)
MEDIANFILTER_SOURCE_DIR ?= $(ROOT)/../ut-ATL24-medianfilter
MEDIANFILTER_SOURCE_FILE = $(MEDIANFILTER_SOURCE_DIR)/medianmodel.py
MEDIANFILTER_VERSION = $(shell git --work-tree ${MEDIANFILTER_SOURCE_DIR} --git-dir ${MEDIANFILTER_SOURCE_DIR}/.git describe --abbrev --dirty --always --tags --long)
COASTNET_DIR = $(ROOT)/../ut-ATL24-coastnet
QTREES_DIR = $(ROOT)/../ut-ATL24-qtrees
OPENOCEANSPP_DIR = $(ROOT)/../ut-ATL24-oopp

INSTALLDIR ?= $(SLIDERULE_STAGE_DIR)

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
GHCR := ghcr.io/slideruleearth
DOCKEROPTS ?=
BUILDX ?= # buildx
DOCKER_PLATFORM ?= # --platform linux/arm64 | linux/amd64
ARCH ?= $(shell arch)
DOMAIN ?= testsliderule.org
DOMAIN_ROOT = $(firstword $(subst ., ,$(DOMAIN)))
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER = $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
USERCFG ?=
DISTRIBUTION_ID = $(shell aws cloudfront list-distributions --query "DistributionList.Items[?Aliases.Items[0]=='$(DOMAIN)'].Id" --output text)

CLANG_VER ?= ""
CLANG_CFG = export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER)

DEBUG_CFG := -DCMAKE_BUILD_TYPE=Debug
DEBUG_CFG += -DINSTALLDIR=$(INSTALLDIR)
DEBUG_CFG += -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
DEBUG_CFG += -DENABLE_ADDRESS_SANITIZER=ON
DEBUG_CFG += $(USERCFG)

RELEASE_CFG := -DCMAKE_BUILD_TYPE=Release
RELEASE_CFG += -DINSTALLDIR=$(INSTALLDIR)
RELEASE_CFG += $(USERCFG)

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
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) IS_PUBLIC=False $(INSTALLDIR)/bin/sliderule $(ROOT)/scripts/apps/server.lua config.json

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

static-website-run: ## locally run the static website; needs VERSION
	docker run -it -p 8080:4000 --rm --name website $(ECR)/static-website:latest

static-website-export: ## export static website to local host directory
	-rm -Rf $(STATIC_WEB_EXPORT_DIR)
	mkdir -p $(STATIC_WEB_EXPORT_DIR)
	docker run -it --entrypoint /bin/bash  -v ./docker/static-website:/host -v $(STATIC_WEB_EXPORT_DIR):/output --rm --name website $(ECR)/static-website:latest /host/export_dist.sh

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

oceaneyes-lock:
	cd $(OCEANEYES_SRC_DIR) && conda-lock -p linux-$(ARCH) -f environment.yml
	cd $(OCEANEYES_SRC_DIR) && conda-lock render -p linux-$(ARCH)

oceaneyes-docker:
	-rm -Rf $(OCEANEYES_STAGE_DIR)
	mkdir -p $(OCEANEYES_STAGE_DIR)
	cp $(OCEANEYES_SRC_DIR)/Dockerfile $(OCEANEYES_STAGE_DIR)
	cp $(OCEANEYES_SRC_DIR)/conda-* $(OCEANEYES_STAGE_DIR)
	cp $(OCEANEYES_SRC_DIR)/runner.* $(OCEANEYES_STAGE_DIR)
	cp $(OCEANEYES_SRC_DIR)/atl24_iso_xml_template.txt $(OCEANEYES_STAGE_DIR)
	mkdir $(OCEANEYES_STAGE_DIR)/cshelph
	cp $(CSHELPH_SOURCE_FILE) $(OCEANEYES_STAGE_DIR)/cshelph
	@echo $(CSHELPH_VERSION) > $(OCEANEYES_STAGE_DIR)/cshelph/cshelph_version.txt
	mkdir $(OCEANEYES_STAGE_DIR)/medianfilter
	cp $(MEDIANFILTER_SOURCE_FILE) $(OCEANEYES_STAGE_DIR)/medianfilter
	@echo $(MEDIANFILTER_VERSION) > $(OCEANEYES_STAGE_DIR)/medianfilter/medianfilter_version.txt
	cp -R $(OCEANEYES_SRC_DIR)/bathypathfinder $(OCEANEYES_STAGE_DIR)
	cp -R $(OCEANEYES_SRC_DIR)/pointnet $(OCEANEYES_STAGE_DIR)
	cp -R $(OCEANEYES_SRC_DIR)/openoceans $(OCEANEYES_STAGE_DIR)
	mkdir $(OCEANEYES_STAGE_DIR)/magic_pytorch_env
	rsync -a $(MAGIC_PYTORCH_DIR)/* $(OCEANEYES_STAGE_DIR)/magic_pytorch_env
	cd $(OCEANEYES_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/oceaneyes:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/oceaneyes:latest $(ECR)/oceaneyes:$(VERSION)
	docker tag $(ECR)/oceaneyes:latest $(ECR)/oceaneyes:$(MAJOR_VERSION)

oceaneyes-push:
	docker push $(ECR)/oceaneyes:$(VERSION)
	docker push $(ECR)/oceaneyes:$(MAJOR_VERSION)

sliderule-buildenv-docker: ## build sliderule build environment docker image
	-rm -Rf $(BUILDENV_STAGE_DIR)
	mkdir -p $(BUILDENV_STAGE_DIR)
	cp docker/sliderule/Dockerfile.buildenv $(BUILDENV_STAGE_DIR)/Dockerfile
	cd $(BUILDENV_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/sliderule-buildenv:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/sliderule-buildenv:latest $(ECR)/sliderule-buildenv:$(VERSION)
	docker tag $(ECR)/sliderule-buildenv:latest $(ECR)/sliderule-buildenv:$(MAJOR_VERSION)
	docker tag $(ECR)/sliderule-buildenv:latest $(GHCR)/sliderule-buildenv:$(VERSION)
	docker run -it -v ./docker/sliderule:/host --rm --name buildenv $(ECR)/sliderule-buildenv:$(VERSION) /bin/bash /host/libdep.sh /host/libdep-$(ARCH).lock

sliderule-buildenv-docker-push: ## push build environment docker image to registry
	docker push $(ECR)/sliderule-buildenv:$(VERSION)
	docker push $(ECR)/sliderule-buildenv:$(MAJOR_VERSION)

sliderule-docker: ## build sliderule docker image using buildenv container; needs VERSION
	-rm -Rf $(SLIDERULE_STAGE_DIR)
	mkdir -p $(SLIDERULE_STAGE_DIR)
	rsync -a $(ROOT) $(SLIDERULE_STAGE_DIR)/sliderule --exclude build --exclude stage
	rsync -a $(COASTNET_DIR) $(SLIDERULE_STAGE_DIR) --exclude models
	rsync -a $(QTREES_DIR) $(SLIDERULE_STAGE_DIR) --exclude models
	rsync -a $(OPENOCEANSPP_DIR) $(SLIDERULE_STAGE_DIR)
	cp docker/sliderule/Dockerfile.runtime $(SLIDERULE_STAGE_DIR)/Dockerfile
	cp docker/sliderule/config.json $(SLIDERULE_STAGE_DIR)
	cd $(SLIDERULE_STAGE_DIR); docker $(BUILDX) build $(DOCKEROPTS) --build-arg repo=$(ECR) -t $(ECR)/sliderule:latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(VERSION)
	docker tag $(ECR)/sliderule:latest $(ECR)/sliderule:$(MAJOR_VERSION)

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/intelligent-load-balancer/* $(ILB_STAGE_DIR)
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
	cd packer; packer build -var version=$(MAJOR_VERSION) -var-file=arm64_cpu.json sliderule-base.pkr.hcl

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
MOSAICS_PERFORMANCE_TEST ?= $(ROOT)/datasets/pgc/systests/arcticdem_mosaic_perf.lua
STRIPS_PERFORMANCE_TEST ?=  $(ROOT)/datasets/pgc/systests/arcticdem_strips_perf.lua
SUBSET_PERFORMANCE_TEST ?=  $(ROOT)/scripts/systests/subset_perf_test.lua

selftest: ## run the self test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(TEST)

shorttest: ## run the short form self test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(ROOT)/scripts/selftests/test_runner.lua short

perfmtest: ## run mosaics performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(MOSAICS_PERFORMANCE_TEST)

perfstest: ## run strips performance test on the server code
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(STRIPS_PERFORMANCE_TEST)

perfsubsetest: ## run subset performance test
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(SUBSET_PERFORMANCE_TEST)

OCEANEYES_SHARE=/data/AppServer.1
oceaneyes-test: ## run oceaneyes
	docker run -it --rm -v /data:/data -v $(OCEANEYES_SHARE):/share --name oceaneyes $(ECR)/oceaneyes:$(VERSION) /bin/bash /runner.sh $(OCEANEYES_SHARE)/settings.json

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