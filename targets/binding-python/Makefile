ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_SOURCE_DIR = $(ROOT)
BINDINGS_BUILD_DIR = $(BUILD)/srpy
BINDINGS_INSTALL_DIR ?= $(STAGE)/srpy

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
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_BEST_EFFORT_CONDA_ENV=ON $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(ROOT)

config-conda: prep ## configure make for python bindings (using conda environment)
	cd $(BINDINGS_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) $(PYTHONCFG) -DINSTALLDIR=$(BINDINGS_INSTALL_DIR) $(ROOT)

bindings: ## build the server using the local configuration
	make -j4 -C $(BINDINGS_BUILD_DIR)
	make -C $(BINDINGS_BUILD_DIR) install

install: ## install bindings into system
	cp $(BINDINGS_INSTALL_DIR)/lib/srpybin.cpython* /usr/local/lib
	chmod 644 /usr/local/lib/srpybin.cpython*

distclean: ## fully remove all non-version controlled files and directories
	make -C $(SLIDERULE_SOURCE_DIR) distclean

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)
