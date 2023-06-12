#
# Notes:
#    There is an issue loading a plugin from the python module initialization; which means in order to get any plugin functionality, the plugin
# 	 has to be linked to the python module.  This creates a chicken-and-egg problem where the plugin needs to binding core library in order to build,
#	 and the bindings core library needs the plugin to build.  The _temporary_ workaround for this is to prebuild the core library and plugin, using
#	 the server build of the core library, and then build the bindings and link against it.
#
ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_SOURCE_DIR = $(ROOT)
BUILDENV_SOURCE_DIR = $(ROOT)/targets/slideruleearth-aws
BINDINGS_BUILD_DIR = $(BUILD)/srpy
SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
ICESAT2_BUILD_DIR = $(BUILD)/icesat2
BINDINGS_INSTALL_DIR ?= $(STAGE)/srpy

VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
ECR := 742127912612.dkr.ecr.us-west-2.amazonaws.com
GHCR := ghcr.io/icesat2-sliderule
DOCKEROPTS ?=

SLIDERULECFG := -DUSE_H5_PACKAGE=ON
SLIDERULECFG += -DUSE_AWS_PACKAGE=ON
SLIDERULECFG += -DUSE_LEGACY_PACKAGE=ON
SLIDERULECFG += -DUSE_CCSDS_PACKAGE=ON
SLIDERULECFG += -DUSE_GEO_PACKAGE=ON
SLIDERULECFG += -DUSE_NETSVC_PACKAGE=ON

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
PYTHONCFG += -DICESAT2_PLUGIN_LIBPATH=$(BINDINGS_INSTALL_DIR)/etc/sliderule/icesat2.so
PYTHONCFG += -DICESAT2_PLUGIN_INCPATH=$(BINDINGS_INSTALL_DIR)/include/sliderule/icesat2

all: bindings

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)
	mkdir -p $(ICESAT2_BUILD_DIR)
	mkdir -p $(BINDINGS_BUILD_DIR)
	echo $(PYTHONCFG)

config: prep ## configure make for python bindings (using system environent)
	cd $(SLIDERULE_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release $(SLIDERULECFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_BEST_EFFORT_CONDA_ENV=ON $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(ROOT)

config-conda: prep ## configure make for python bindings (using conda environment)
	cd $(SLIDERULE_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) -DSERVER_APP=OFF $(SLIDERULECFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(ROOT)

bindings: ## build the server using the local configuration
	make -j4 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install
	make -j4 -C $(ICESAT2_BUILD_DIR)
	make -C $(ICESAT2_BUILD_DIR) install
	make -j4 -C $(BINDINGS_BUILD_DIR)
	make -C $(BINDINGS_BUILD_DIR) install

install: ## install bindings into system
	cp $(BINDINGS_INSTALL_DIR)/lib/srpybin.cpython* /usr/local/lib
	chmod 644 /usr/local/lib/srpybin.cpython*

bindings-docker: ## build srpy docker image using buildenv container; needs VERSION
	-rm -Rf $(BINDINGS_INSTALL_DIR)
	mkdir -p $(BINDINGS_INSTALL_DIR)/etc
	rsync -a $(SLIDERULE_SOURCE_DIR) $(BINDINGS_INSTALL_DIR)/srpy --exclude build --exclude stage
	cp Dockerfile $(BINDINGS_INSTALL_DIR)
	cd $(BINDINGS_INSTALL_DIR); docker build $(DOCKEROPTS) --build-arg repo=$(GHCR) -t $(ECR)/srpy:latest .
	docker tag $(ECR)/srpy:latest $(ECR)/srpy:$(VERSION)
	docker tag $(ECR)/srpy:latest $(ECR)/srpy:$(MAJOR_VERSION)

bindings-full: buildenv-docker bindings-docker ## build srpy docker container from scratch; needs VERSION

bindings-push: ## push all cluster images to docker container registries
	docker push $(ECR)/srpy:$(VERSION)
	docker push $(ECR)/srpy:$(MAJOR_VERSION)

bindings-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(ECR)/srpy:$(VERSION)
	- docker system prune -f

buildenv-docker: ## build sliderule build environment docker image
	make -C $(BUILDENV_SOURCE_DIR) sliderule-buildenv-docker

distclean: ## fully remove all non-version controlled files and directories
	make -C $(SLIDERULE_SOURCE_DIR) distclean

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)
