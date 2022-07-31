ROOT = $(shell pwd)
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage
SERVER_BUILD_DIR = $(BUILD)/sliderule
PLUGIN_BUILD_DIR = $(BUILD)/plugin
SERVER_STAGE_DIR = $(STAGE)/sliderule
MONITOR_STAGE_DIR = $(STAGE)/monitor
ORCHESTRATOR_STAGE_DIR = $(STAGE)/orchestrator

# when using the llvm toolchain to build the source
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

# for a MacOSX host to have this ip command you must install homebrew(see https://brew.sh/) then run 'brew install iproute2mac' on your mac host
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))

######################
# Development Targets
######################

all: default-build

default-build: ## default build of sliderule
	make -j4 -C $(BUILD)

config: release-config ## configure make for default build

release-config: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

debug-config: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)

DEVCFG  = -DENABLE_TRACING=ON
DEVCFG += -DUSE_AWS_PACKAGE=ON
DEVCFG += -DUSE_CCSDS_PACKAGE=ON
DEVCFG += -DUSE_GEOTIFF_PACKAGE=ON
DEVCFG += -DUSE_H5_PACKAGE=ON
DEVCFG += -DUSE_LEGACY_PACKAGE=ON
DEVCFG += -DUSE_NETSVC_PACKAGE=ON
DEVCFG += -DUSE_PISTACHE_PACKAGE=ON

development-config: prep ## configure make for debug version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) $(ROOT)

PYTHONCFG  = -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=ON
PYTHONCFG += -DUSE_CCSDS_PACKAGE=ON
PYTHONCFG += -DUSE_GEOTIFF_PACKAGE=ON
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192
PYTHONCFG += -DICESAT2_PLUGIN_LIBPATH=/usr/local/etc/sliderule/icesat2.so
PYTHONCFG += -DICESAT2_PLUGIN_INCPATH=/usr/local/include/sliderule

python-config: prep ## configure make for python bindings
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

library-config: prep ## configure make for shared library libsliderule.so
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

atlas-config: prep ## configure make for atlas plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/atlas

icesat2-config: prep ## configure make for icesat2 plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/icesat2

scan: prep ## perform static analysis
	cd $(BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(DEVCFG) $(ROOT)
	cd $(BUILD); scan-build -o scan-results make

asan: prep ## build address sanitizer debug version of sliderule binary
	cd $(BUILD); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(DEVCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(BUILD); make

ctags: prep ## generate ctags
	cd $(BUILD); cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)

install: ## install sliderule to system
	make -C $(BUILD) install

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(BUILD)/install_manifest.txt

testmem: ## run memory test on sliderule
	valgrind --leak-check=full --track-origins=yes --track-fds=yes sliderule $(testcase)

testcpu: ## run cpu test on sliderule
	valgrind --tool=callgrind sliderule $(testcase)
	# kcachegrind callgrind.out.<pid>

testheap: ## run heap test on sliderule
	valgrind --tool=massif --time-unit=B --pages-as-heap=yes sliderule $(testcase)
	# ms_print massif.out.<pid>

testheaptrack: ## analyze results of heap test
	# heaptrack sliderule $(testcase)
	# heaptrack_gui heaptrack.sliderule.<pid>.gz

testcov: ## analyze results of test coverage report
	lcov -c --directory $(BUILD) --output-file $(BUILD)/coverage.info
	genhtml $(BUILD)/coverage.info --output-directory $(BUILD)/coverage_html
	# firefox $(BUILD)/coverage_html/index.html

testpy: ## run python binding test
	cp scripts/systests/coro.py $(BUILD)
	cd $(BUILD); /usr/bin/python3 coro.py

#################
# Server Targets
#################

SLIDERULE_DOCKER_TAG ?= icesat2sliderule/sliderule:latest

SERVERCFG := -DMAX_FREE_STACK_SIZE=1
SERVERCFG += -DUSE_AWS_PACKAGE=ON
SERVERCFG += -DUSE_H5_PACKAGE=ON
SERVERCFG += -DUSE_NETSVC_PACKAGE=ON
SERVERCFG += -DUSE_GEOTIFF_PACKAGE=ON
SERVERCFG += -DUSE_LEGACY_PACKAGE=OFF
SERVERCFG += -DUSE_CCSDS_PACKAGE=OFF

server: ## build the server using the local configuration
	make -j4 -C $(SERVER_BUILD_DIR)
	make -C $(SERVER_BUILD_DIR) install
	make -j4 -C $(PLUGIN_BUILD_DIR)
	make -C $(PLUGIN_BUILD_DIR) install
	cp targets/icesat2-sliderule-docker/asset_directory.csv $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/empty.index $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/plugins.conf $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/earth_data_auth.lua $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/service_registry.lua $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/proxy.lua $(SERVER_STAGE_DIR)/etc/sliderule/api

server-config-debug: ## configure the server for running locally with debug symbols
	mkdir -p $(SERVER_BUILD_DIR)
	mkdir -p $(PLUGIN_BUILD_DIR)
	cd $(SERVER_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug $(SERVERCFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)
	cd $(PLUGIN_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)/plugins/icesat2

server-config-release: ## configure server to run release version locally (useful for using valgrind)
	mkdir -p $(SERVER_BUILD_DIR)
	mkdir -p $(PLUGIN_BUILD_DIR)
	cd $(SERVER_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release $(SERVERCFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)
	cd $(PLUGIN_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)/plugins/icesat2

server-config-asan: ## configure server to run with address sanitizer locally
	mkdir -p $(SERVER_BUILD_DIR)
	mkdir -p $(PLUGIN_BUILD_DIR)
	cd $(SERVER_BUILD_DIR); export CC=clang; export CXX=clang++; cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON $(SERVERCFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)
	cd $(PLUGIN_BUILD_DIR); export CC=clang; export CXX=clang++; cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)/plugins/icesat2

server-run: ## run the server locally
	IPV4=$(MYIP) $(SERVER_STAGE_DIR)/bin/sliderule targets/icesat2-sliderule-docker/server.lua targets/icesat2-sliderule-docker/config-development.json

server-run-valgrind: ## run the server in valgrind
	IPV4=$(MYIP) valgrind --leak-check=full --track-origins=yes --track-fds=yes $(SERVER_STAGE_DIR)/bin/sliderule targets/icesat2-sliderule-docker/server.lua targets/icesat2-sliderule-docker/config-development.json

server-docker: distclean ## build the server docker container
	# build and install sliderule into staging
	mkdir -p $(SERVER_BUILD_DIR)
	cd $(SERVER_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release $(SERVERCFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) -DRUNTIMEDIR=/usr/local/etc/sliderule $(ROOT)
	make -j4 $(SERVER_BUILD_DIR)
	make -C $(SERVER_BUILD_DIR) install
	# build and install plugin into staging
	mkdir -p $(PLUGIN_BUILD_DIR)
	cd $(PLUGIN_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(ROOT)/plugins/icesat2
	make -j4 $(PLUGIN_BUILD_DIR)
	make -C $(PLUGIN_BUILD_DIR) install
	# copy over dockerfile
	cp targets/icesat2-sliderule-docker/Dockerfile $(SERVER_STAGE_DIR)
	cp targets/icesat2-sliderule-docker/plugins.conf $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/config-production.json $(SERVER_STAGE_DIR)/etc/sliderule/config.json
	cp targets/icesat2-sliderule-docker/asset_directory.csv $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/empty.index $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/server.lua $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/earth_data_auth.lua $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/service_registry.lua $(SERVER_STAGE_DIR)/etc/sliderule
	cp targets/icesat2-sliderule-docker/proxy.lua $(SERVER_STAGE_DIR)/etc/sliderule/api
	# copy over entry point
	mkdir -p $(SERVER_STAGE_DIR)/scripts
	cp targets/icesat2-sliderule-docker/docker-entrypoint.sh $(SERVER_STAGE_DIR)/scripts
	chmod +x $(SERVER_STAGE_DIR)/scripts/docker-entrypoint.sh
	# build image
	cd $(SERVER_STAGE_DIR); docker build -t $(SLIDERULE_DOCKER_TAG) .

server-docker-run: ## run the server in a docker container
	docker run -it --rm --name=sliderule-app -e IPV4=$(MYIP) -v /etc/ssl/certs:/etc/ssl/certs -v /data:/data -p 9081:9081 --entrypoint /usr/local/scripts/docker-entrypoint.sh $(SLIDERULE_DOCKER_TAG)

##################
# Monitor Targets
##################

MONITOR_DOCKER_TAG ?= icesat2sliderule/monitor:latest

monitor-docker: distclean ## build monitor docker image
	mkdir -p $(MONITOR_STAGE_DIR)
	cp targets/icesat2-monitor-docker/* $(MONITOR_STAGE_DIR)
	chmod +x $(MONITOR_STAGE_DIR)/docker-entrypoint.sh
	cd $(MONITOR_STAGE_DIR); docker build -t $(MONITOR_DOCKER_TAG) .

monitor-docker-run: ## run monitor docker container
	docker run -it --rm --name=monitor -p 3000:3000 -p 3100:3100 -p 9090:9090 --entrypoint /usr/local/etc/docker-entrypoint.sh $(MONITOR_DOCKER_TAG)

#######################
# Orhcestrator Targets
#######################

ORCHESTRATOR_DOCKER_TAG ?= icesat2sliderule/orchestrator:latest

orchestrator-docker: distclean ## build orchestrator docker image
	mkdir -p $(ORCHESTRATOR_STAGE_DIR)
	cp targets/icesat2-orchestrator-docker/* $(ORCHESTRATOR_STAGE_DIR)
	cd $(ORCHESTRATOR_STAGE_DIR); docker build -t $(ORCHESTRATOR_DOCKER_TAG) .

orchestrator-docker-run: ## run orchestrator docker container
	docker run -it --rm --name=orchestrator -p 8050:8050 $(ORCHESTRATOR_DOCKER_TAG)

####################
# Global Targets
####################

prep: ## create necessary build directories
	mkdir -p $(BUILD)
	mkdir -p $(SERVER_BUILD_DIR)
	mkdir -p $(PLUGIN_BUILD_DIR)

clean: ## clean last build
	- make -C $(BUILD) clean
	- make -C $(SERVER_BUILD_DIR) clean
	- make -C $(PLUGIN_BUILD_DIR) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

