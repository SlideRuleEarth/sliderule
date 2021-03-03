ROOT = $(shell pwd)
STAGEDIR = stage
RUNDIR = /usr/local/etc/sliderule
SLIDERULE = $(ROOT)/../..
DOCKERTAG ?= icesat2sliderule/sliderule:latest

ICESAT2CFG := -DMAX_FREE_STACK_SIZE=1
ICESAT2CFG += -DUSE_AWS_PACKAGE=ON
ICESAT2CFG += -DUSE_H5_PACKAGE=ON
ICESAT2CFG += -DUSE_ICESAT2_PLUGIN=ON
ICESAT2CFG += -DUSE_LEGACY_PACKAGE=OFF
ICESAT2CFG += -DUSE_CCSDS_PACKAGE=OFF
ICESAT2CFG += -DUSE_ATLAS_PLUGIN=OFF

all: default-build

default-build:
	make -j4 -C build

config: development-config

development-config:
	mkdir -p build
	cd build; cmake -DCMAKE_BUILD_TYPE=Debug $(ICESAT2CFG) $(SLIDERULE)

trace-config:
	mkdir -p build
	cd build; cmake -DCMAKE_BUILD_TYPE=Debug $(ICESAT2CFG) -DENABLE_LTTNG_TRACING=ON $(SLIDERULE)

production-config:
	mkdir -p build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release $(ICESAT2CFG) -DINSTALLDIR=$(ROOT)/$(STAGEDIR) -DRUNTIMEDIR=$(RUNDIR) $(SLIDERULE)

docker-image: distclean production-config default-build
	# install sliderule #
	make -C build install
	# copy over dockerfile #
	cp $(SLIDERULE)/targets/sliderule-docker/dockerfile.app $(STAGEDIR)/Dockerfile
	# copy over installation configuration #
	cp build/plugins.conf $(STAGEDIR)/etc/sliderule
	cp config.json $(STAGEDIR)/etc/sliderule
	cp asset_directory.csv $(STAGEDIR)/etc/sliderule
	cp empty.index $(STAGEDIR)/etc/sliderule
	# copy over scripts #
	mkdir $(STAGEDIR)/scripts
	cp -R $(SLIDERULE)/scripts/apps $(STAGEDIR)/scripts/apps
	cp -R $(SLIDERULE)/scripts/tests $(STAGEDIR)/scripts/tests
	cp docker-entrypoint.sh $(STAGEDIR)/scripts/apps
	chmod +x $(STAGEDIR)/scripts/apps/docker-entrypoint.sh
	# copy over plugin tests #
	mkdir -p $(STAGEDIR)/plugins/icesat2/tests
	cp $(SLIDERULE)/plugins/icesat2/tests/* $(STAGEDIR)/plugins/icesat2/tests
	# copy over plugin apps #
	mkdir -p $(STAGEDIR)/plugins/icesat2/apps
	cp $(SLIDERULE)/plugins/icesat2/apps/* $(STAGEDIR)/plugins/icesat2/apps
	# build image #
	cd $(STAGEDIR); docker build -t $(DOCKERTAG) .

run-docker:
	docker run -it --rm --name=sliderule-app -v /etc/ssl/certs:/etc/ssl/certs -v /data:/data -p 9081:9081 --entrypoint /usr/local/scripts/apps/docker-entrypoint.sh $(DOCKERTAG)

run-stand-alone:
	sliderule $(SLIDERULE)/plugins/icesat2/apps/server.lua config.json

run-memory-test:
	valgrind --leak-check=full --track-origins=yes --track-fds=yes sliderule $(SLIDERULE)/plugins/icesat2/apps/server.lua config.json

install:
	make -C build install
	cp asset_directory.csv $(RUNDIR)
	cp empty.index $(RUNDIR)

clean:
	make -C build clean
	- rm aws_sdk_*.log

distclean:
	- rm -Rf build
	- rm -Rf $(STAGEDIR)
