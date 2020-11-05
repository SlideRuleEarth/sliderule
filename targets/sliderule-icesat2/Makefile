ROOT = $(shell pwd)
STAGEDIR = stage
RUNDIR = /usr/local/etc/sliderule
SLIDERULE = $(ROOT)/../..
ICESAT2CFG = -DENABLE_H5_REST_VOL=ON -DUSE_AWS_PACKAGE=ON -DUSE_H5_PACKAGE=ON -DUSE_ICESAT2_PLUGIN=ON -DUSE_LEGACY_PACKAGE=OFF -DUSE_CCSDS_PACKAGE=OFF -DUSE_SIGVIEW_PLUGIN=OFF
DOCKERTAG = icesat2sliderule/sliderule:master
HSDS_IP?=$(shell (getent hosts hsds.sliderule.beta | awk '{ print $$1 ; exit }'))

all: default-build

default-build:
	make -j4 -C build

config: development-config

development-config:
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
	cp plugins.conf $(STAGEDIR)/etc/sliderule
	cp config.json $(STAGEDIR)/etc/sliderule
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
	# copy over library dependencies #
	mkdir -p $(STAGEDIR)/lib
	cp /usr/local/lib/libhdf5_vol_rest.so $(STAGEDIR)/lib
	cp /usr/local/lib/libyajl.so* $(STAGEDIR)/lib
	# build image #
	cd $(STAGEDIR); docker build -t $(DOCKERTAG) .

run-docker:
	# note: /data must be populated with a `config` directory containing the asset_directory.csv and any corresponding index files
	docker run -it --rm --name=sliderule-app -v /etc/ssl/certs:/etc/ssl/certs -v /data:/data -p 9081:9081 -e HSDS_ENDPOINT=http://$(HSDS_IP) -e HSDS_USERNAME=admin -e HSDS_PASSWORD=0m3g4 --entrypoint /usr/local/scripts/apps/docker-entrypoint.sh $(DOCKERTAG)

run-stand-alone:
	HSDS_ENDPOINT=http://$(HSDS_IP) HSDS_USERNAME=admin HSDS_PASSWORD=0m3g4 sliderule $(SLIDERULE)/plugins/icesat2/apps/proxy.lua config.json 35100 &
	HSDS_ENDPOINT=http://$(HSDS_IP) HSDS_USERNAME=admin HSDS_PASSWORD=0m3g4 sliderule $(SLIDERULE)/plugins/icesat2/apps/server.lua config.json

install:
	make -C build install
	cp plugins.conf $(RUNDIR)

clean:
	make -C build clean
	- rm aws_sdk_*.log

distclean:
	- rm -Rf build
	- rm -Rf $(STAGEDIR)
	- rm -Rf .cache
