# Makefile to build sliderule with atlas plugin

ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
SLIDERULE_STAGE_DIR = $(STAGE)/atlas
ATLAS_BUILD_DIR = $(BUILD)/altas

CLANG_VER = "-12"
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
CLANG_CFG = export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER)

PKG_CFG += -DUSE_ARROW_PACKAGE=OFF
PKG_CFG += -DUSE_AWS_PACKAGE=OFF
PKG_CFG += -DUSE_CCSDS_PACKAGE=ON
PKG_CFG += -DUSE_GEO_PACKAGE=ON
PKG_CFG += -DUSE_H5_PACKAGE=ON
PKG_CFG += -DUSE_LEGACY_PACKAGE=ON
PKG_CFG += -DUSE_NETSVC_PACKAGE=ON
PKG_CFG += -DUSE_PISTACHE_PACKAGE=OFF

DEBUG_CFG := -DCMAKE_BUILD_TYPE=Debug
DEBUG_CFG += -DINSTALLDIR=$(SLIDERULE_STAGE_DIR)
DEBUG_CFG += -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
DEBUG_CFG += -DENABLE_ADDRESS_SANITIZER=ON

RELEASE_CFG := -DCMAKE_BUILD_TYPE=Release
RELEASE_CFG += -DINSTALLDIR=$(SLIDERULE_STAGE_DIR)

########################
# Build Targets
########################

all: atlas

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)
	mkdir -p $(SLIDERULE_STAGE_DIR)
	mkdir -p $(ATLAS_BUILD_DIR)

config-debug: prep ## configure the server for running locally with debug symbols, optimizations off, static analysis, and address sanitizer
	cd $(SLIDERULE_BUILD_DIR) && $(CLANG_CFG) && cmake $(PKG_CFG) $(DEBUG_CFG) $(ROOT)
	cd $(ATLAS_BUILD_DIR)     && $(CLANG_CFG) && cmake            $(DEBUG_CFG) $(ROOT)/plugins/atlas

config-release: prep ## configure server to run a release version locally
	cd $(SLIDERULE_BUILD_DIR) && cmake $(PKG_CFG) $(RELEASE_CFG) $(ROOT)
	cd $(ATLAS_BUILD_DIR)     && cmake            $(RELEASE_CFG) $(ROOT)/plugins/atlas

atlas: ## build the server using the local configuration
	make -j4 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install
	make -j4 -C $(ATLAS_BUILD_DIR)
	make -C $(ATLAS_BUILD_DIR) install

run: ## run the server locally
	$(SLIDERULE_STAGE_DIR)/bin/sliderule $(ROOT)/plugins/atlas/apps/server.lua $(ROOT)/plugins/atlas/config/config.json

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)

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
