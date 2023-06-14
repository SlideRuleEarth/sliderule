ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
DOCKEROPTS ?=

SLIDERULE_SOURCE_DIR = $(ROOT)
BINDINGS_BUILD_DIR = $(BUILD)/srpy
BINDINGS_STAGE_DIR ?= $(STAGE)/srpy
INSTALL_DIR ?= /usr/local/lib

PYTHONCFG := -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=ON
PYTHONCFG += -DUSE_CCSDS_PACKAGE=ON
PYTHONCFG += -DUSE_GEO_PACKAGE=ON
PYTHONCFG += -DUSE_NETSVC_PACKAGE=ON
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192

all: bindings

prep: ## create temporary directories needed for build
	mkdir -p $(BINDINGS_BUILD_DIR)

config: prep ## configure make for python bindings (using system environent)
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_BEST_EFFORT_CONDA_ENV=ON $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_STAGE_DIR) $(ROOT)

config-conda: prep ## configure make for python bindings (using conda environment)
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_STAGE_DIR) $(ROOT)

bindings: ## build the bindings
	make -j4 -C $(BINDINGS_BUILD_DIR)
	make -C $(BINDINGS_BUILD_DIR) install

install: ## install bindings into system
	cp $(BINDINGS_STAGE_DIR)/lib/srpybin.cpython* $(INSTALL_DIR)
	chmod 644 $(INSTALL_DIR)/srpybin.cpython*

bindings-docker: prep ## build docker container to run the bindings
	mkdir -p $(BINDINGS_STAGE_DIR)/sliderule
	rsync -a $(SLIDERULE_SOURCE_DIR) $(BINDINGS_STAGE_DIR)/sliderule --exclude build --exclude stage
	cp Dockerfile $(BINDINGS_STAGE_DIR)/Dockerfile
	cd $(BINDINGS_STAGE_DIR); docker build --progress=plain $(DOCKEROPTS) -t $(ECR)/srpy:latest .
	docker tag $(ECR)/srpy:latest $(ECR)/srpy:$(VERSION)
	docker tag $(ECR)/srpy:latest $(ECR)/srpy:$(MAJOR_VERSION)

bindings-docker-run: ## run the docker container locally; needs VERSION
	docker run -it --rm --name=python-bindings $(ECR)/srpy:$(VERSION)

distclean: ## fully remove all non-version controlled files and directories
	make -C $(SLIDERULE_SOURCE_DIR) distclean

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
