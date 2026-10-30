ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage
VERSION ?= latest
ATL24_BUILD = $(BUILD)/atl24
ATL24_STAGE = $(BUILD)/stage
ATL24_ALGO ?= $(ROOT)/../atl24_v2_algorithms
BUCKET ?= s3://sliderule
CONTAINER_REGISTRY ?= 742127912612.dkr.ecr.us-west-2.amazonaws.com
ENVVER ?= $(shell cd ../../ && git describe --abbrev --dirty --always --tags --long)
TEST_GRANULE = ATL03_20181028071900_04530107_007_01.h5
PROJECT_BUCKET = sliderule
PROJECT_FOLDER = cf
AWS_REGION = us-west-2
MAKECFG ?= -DCMAKE_CXX_COMPILER=gcc14-g++
USERCFG ?=
CYCLE ?=

all:
	make -j8 -C $(ATL24_BUILD)

config: prep
	cd $(ATL24_BUILD) && \
	cmake -DCMAKE_BUILD_TYPE=Release -DATL24DIR=$(ATL24_ALGO) $(USERCFG) $(MAKECFG) $(ROOT)/targets/atl24

config-stage-debug: prep
	cd $(ATL24_BUILD) && \
	cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(ROOT)/stage/sliderule -DATL24DIR=$(ATL24_ALGO) $(USERCFG) $(MAKECFG) $(ROOT)/targets/atl24

config-stage-release: prep
	cd $(ATL24_BUILD) && \
	cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(ROOT)/stage/sliderule -DATL24DIR=$(ATL24_ALGO) $(USERCFG) $(MAKECFG) $(ROOT)/targets/atl24

install:
	make -C $(ATL24_BUILD) install

uninstall:
	xargs rm < $(ATL24_BUILD)/install_manifest.txt

prep:
	mkdir -p $(ATL24_BUILD)

selftest: install
	make -C $(ROOT)/targets/slideruleearth run RUN_CMD=$(ROOT)/targets/atl24/selftests/test_runner.lua

docker-atl24:
	-rm -Rf $(ATL24_STAGE)
	mkdir -p $(ATL24_STAGE)
	rsync -a $(ROOT) $(ATL24_STAGE)/sliderule --exclude build --exclude stage
	rsync -a $(ATL24_ALGO) $(ATL24_STAGE) --exclude build --exclude stage
	cp docker/atl24/Dockerfile $(ATL24_STAGE)
	cp docker/atl24/docker-entrypoint.sh $(ATL24_STAGE)
	cd $(ATL24_STAGE) && docker build --build-arg repo=$(CONTAINER_REGISTRY) -t $(CONTAINER_REGISTRY)/sliderule:atl24 .

docker-atl24-push:
	docker push $(CONTAINER_REGISTRY)/sliderule:atl24

sips-run: install
	LOG_FORMAT=FMT_TEXT \
	ENVIRONMENT_VERSION=$(ENVVER) \
	IPv4=localhost \
	$(STAGE)/sliderule/bin/sliderule scripts/sips_atl24r3.lua $(TEST_GRANULE) /tmp

test-docker-run:
	docker run \
		--network host \
		-v /data:/data \
		-v $(ROOT):$(ROOT) \
		-e IPV4=localhost \
		-e LOG_FORMAT=FMT_TEXT \
		-e ENVIRONMENT_VERSION=$(ENVVER) \
		-e PROJECT_BUCKET=$(PROJECT_BUCKET) \
		-e PROJECT_FOLDER=$(PROJECT_FOLDER) \
		-e PROJECT_REGION=$(AWS_REGION) \
		-e ORCHESTRATOR=http://127.0.0.1:8050 \
		-e CLUSTER=localhost \
		-e DOMAIN=localhost \
		-e AMS=http://127.0.0.1:9082 \
		-e CONTAINER_REGISTRY=$(CONTAINER_REGISTRY) \
		--name atl24 --rm \
		$(CONTAINER_REGISTRY)/sliderule:runner \
		/usr/local/etc/sliderule/job_runner.lua $(ROOT)/targets/atl24/scripts/gen_atl24r3.lua $(TEST_GRANULE) /tmp

test-local-run: install
	make -C $(ROOT)/targets/slideruleearth job ARGS="$(ROOT)/targets/atl24/scripts/gen_atl24r3.lua $(TEST_GRANULE) /tmp"

test-vset-run:
	sliderule-runner submit atl24r3_vset scripts/gen_atl24r3.lua data/atl24r3_validation_set.txt --image sliderule:atl24

platinum-run: # needs CYCLE
	python scripts/gen_cycle_dataset.py $(CYCLE)
	sliderule-runner submit atl24r3_cycle_$(CYCLE) scripts/gen_atl24r3.lua data/atl03_granules_cycle_$(CYCLE).txt --image sliderule:atl24

platinum-rerun:
	sliderule-runner scrape --status error --output /data/ATL24/atl24r3_errors.txt
	sliderule-runner submit atl24r3_rerun scripts/gen_atl24r3.lua /data/ATL24/atl24r3_errors.txt --image sliderule:atl24 --memory 24000 --vcpus 8

platinum-check: # needs CYCLE
	python scripts/check_atl24r3.py --atl03_granules data/atl03_granules_cycle_$(CYCLE).txt

platinum-stat:
	sliderule-runner submit atl24r3_stat scripts/stat_atl24r3.py /data/ATL24/atl03_v3_granules.txt --vcpus 1 --memory 8000 --image pysr:unstable

platinum-summarize:
	sliderule-runner scrape --verbose --status success --output /tmp/atl24r3_stats.txt
	python scripts/summarize_stats.py --result_output /tmp/atl24r3_stats.txt

NUM_GRANULES ?= 0
platinum-transfer: # needs NUM_GRANULES
	scripts/sync_atl24r3.sh
	python scripts/transfer_atl24r3.py --transfer $(NUM_GRANULES)

clean:
	- make -C $(ATL24_BUILD) clean

distclean:
	- rm -Rf $(ATL24_BUILD)
