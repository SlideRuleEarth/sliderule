ROOT = $(shell pwd)/..
STAGE = $(ROOT)/stage

OCEANEYES_CONTAINER_STAGE_DIR = $(STAGE)/oceaneyes
OCEANEYES_CONTAINER_NAME ?= oceaneyes

MAGIC_PYTORCH_DIR = $(ROOT)/../miniconda3/envs/pointnet2

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
DOCKEROPTS ?=
BUILDX ?= # buildx
DOCKER_PLATFORM ?= # --platform linux/arm64 | linux/amd64
ARCH ?= $(shell arch)

########################
# Build Targets
########################

lock:
	conda-lock -p linux-$(ARCH) -f environment.yml
	conda-lock render -p linux-$(ARCH)

prep: clean
	mkdir -p $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp Dockerfile $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp conda-* $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R atl24writer $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R bathypathfinder $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R cshelph $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R medianfilter $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R pointnet $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R openoceans $(OCEANEYES_CONTAINER_STAGE_DIR)
	cp -R utils $(OCEANEYES_CONTAINER_STAGE_DIR)
	mkdir $(OCEANEYES_CONTAINER_STAGE_DIR)/magic_pytorch_env
	rsync -a $(MAGIC_PYTORCH_DIR)/* $(OCEANEYES_CONTAINER_STAGE_DIR)/magic_pytorch_env

docker: prep
	cd $(OCEANEYES_CONTAINER_STAGE_DIR) && docker $(BUILDX) build $(DOCKEROPTS) -t $(ECR)/$(OCEANEYES_CONTAINER_NAME):latest $(DOCKER_PLATFORM) .
	docker tag $(ECR)/$(OCEANEYES_CONTAINER_NAME):latest $(ECR)/$(OCEANEYES_CONTAINER_NAME):$(VERSION)
	docker tag $(ECR)/$(OCEANEYES_CONTAINER_NAME):latest $(ECR)/$(OCEANEYES_CONTAINER_NAME):$(MAJOR_VERSION)

push:
	docker push $(ECR)/$(OCEANEYES_CONTAINER_NAME):$(VERSION)
	docker push $(ECR)/$(OCEANEYES_CONTAINER_NAME):$(MAJOR_VERSION)

clean:
	-rm -Rf $(OCEANEYES_CONTAINER_STAGE_DIR)

distclean:
	- rm -Rf $(STAGE)

########################
# Test Targets
########################

ATL24_TEST_DATA_DIR ?= /data/AppServer.1
ATL24_ALGORITHM ?= cshelph

test: ## run the oceaneyes container
	docker run -it --rm -v /data:/data --name $(OCEANEYES_CONTAINER_NAME) $(ECR)/$(OCEANEYES_CONTAINER_NAME):$(VERSION) /env/bin/python /$(ATL24_ALGORITHM)/runner.py $(ATL24_TEST_DATA_DIR)/bathy_spot_3.json $(ATL24_TEST_DATA_DIR)/bathy_spot_3.csv
