# Note: In order to complete the release process, there are a couple of utilities that must be installed.
# 	* GitHub command line client (gh)
# 		See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#		Then the user must authenticate to GitHub via `gh auth login`
#	* AWS command line client (awscli)
#		See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#		Then the user must have up-to-date aws credentials and also logged into the AWS Elastic Container Registry
#	* Terraform & Packer

ROOT = $(shell pwd)/../..
STAGE = $(ROOT)/stage
STATIC_WEB_SOURCE_DIR = $(ROOT)/docs
STATIC_WEB_STAGE_DIR = $(STAGE)/website
SLIDERULE_SOURCE_DIR = $(ROOT)
PYTHON_SOURCE_DIR = $(ROOT)/../sliderule-python
SERVER_STAGE_DIR = $(STAGE)/sliderule
MONITOR_STAGE_DIR = $(STAGE)/monitor
ILB_STAGE_DIR = $(STAGE)/ilb
TF_STAGE_DIR = $(STAGE)/tf
VERSION ?= latest
VERSION_TOKENS := $(subst ., ,$(lastword $(VERSION)))
MAJOR_VERSION := $(word 1,$(VERSION_TOKENS))
REPO ?= 742127912612.dkr.ecr.us-west-2.amazonaws.com
DOCKEROPTS ?=
ARCH ?= arm
ENVVER = $(shell git --git-dir .git --work-tree . describe --abbrev --dirty --always --tags --long)
DOMAIN ?= testsliderule.org
DOMAIN_ROOT = $(firstword $(subst ., ,$(DOMAIN)))
DOMAIN_NAME = $(subst .,-,$(DOMAIN))

all: help

static-website-docker: ## make the static website docker image; needs VERSION
	-rm -Rf $(STATIC_WEB_STAGE_DIR)
	mkdir -p $(STATIC_WEB_STAGE_DIR)
	cd $(STATIC_WEB_SOURCE_DIR) && make distclean
	cp -R $(STATIC_WEB_SOURCE_DIR)/rtd $(STATIC_WEB_STAGE_DIR)
	cp -R $(STATIC_WEB_SOURCE_DIR)/jekyll $(STATIC_WEB_STAGE_DIR)
	cp -R $(PYTHON_SOURCE_DIR) $(STATIC_WEB_STAGE_DIR)
	-cp $(PYTHON_SOURCE_DIR)/examples/* $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp $(PYTHON_SOURCE_DIR)/environment.yml $(STATIC_WEB_STAGE_DIR)/rtd/source/assets
	-cp -R /data/web $(STATIC_WEB_STAGE_DIR)/rtd/source/_static/html
	cp docker/static-website/Dockerfile.$(ARCH) $(STATIC_WEB_STAGE_DIR)/Dockerfile
	cd $(STATIC_WEB_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(REPO)/static-website:latest .
	docker tag $(REPO)/static-website:latest $(REPO)/static-website:$(VERSION)

static-website-run: ## run the static website docker locally; needs VERSION
	docker run -it --rm --name=website -p 4000:4000 $(REPO)/static-website:$(VERSION)

static-website-push:
	docker push  $(REPO)/static-website:$(VERSION)

static-website-deploy: ## deploy static-website using terraform; needs VERSION, DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform apply -var docker_image_url_static-website=$(REPO)/static-website:$(VERSION) -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

static-website-destroy: ## destroy static-website using terraform; needs DOMAIN
	cd terraform/static-website && terraform init
	cd terraform/static-website && terraform workspace select $(DOMAIN)-static-website || terraform workspace new $(DOMAIN)-static-website
	cd terraform/static-website && terraform destroy -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

demo-docker: # make the python client demo docker image; needs VERSION
	-rm -Rf $(DEMO_STAGE_DIR)
	mkdir -p $(DEMO_STAGE_DIR)
	cp -R $(PYTHON_SOURCE_DIR) $(DEMO_STAGE_DIR)
	cp docker/demo/* $(DEMO_STAGE_DIR)
	chmod +x $(DEMO_STAGE_DIR)/docker-entrypoint.sh
	cd $(DEMO_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(REPO)/demo-client:latest .
	docker tag $(REPO)/demo-client:latest $(REPO)/demo-client:$(VERSION)
	mkdir -p $(DEMO_NGINX_STAGE_DIR)
	cp docker/demo-nginx/* $(DEMO_NGINX_STAGE_DIR)
	cd $(DEMO_NGINX_STAGE_DIR) && docker build $(DOCKEROPTS) -t $(REPO)/demo-nginx:latest .
	docker tag $(REPO)/demo-nginx:latest $(REPO)/demo-nginx:$(VERSION)

demo-run: ## run the python client demo docker container locally; needs VERSION
	docker run -it --rm --name=python-app -p 8866:8866 --entrypoint /usr/local/etc/docker-entrypoint.sh $(REPO)/demo-client:$(VERSION)

demo-push:
	docker push $(REPO)/demo-client:$(VERSION)
	docker push $(REPO)/demo-nginx:$(VERSION)

demo-deploy: ## deploy demo using terraform; needs VERSION, DOMAIN
	cd terraform/demo && terraform init
	cd terraform/demo && terraform workspace select $(DOMAIN)-demo || terraform workspace new $(DOMAIN)-demo
	cd terraform/demo && terraform apply -var docker_image_url_demo-client=$(REPO)/demo-client:$(VERSION) -var docker_image_url_demo-nginx=$(REPO)/demo-nginx:$(VERSION) -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

demo-destroy: ## destroy demo using terraform; needs DOMAIN
	cd terraform/demo && terraform init
	cd terraform/demo && terraform workspace select $(DOMAIN)-demo || terraform workspace new $(DOMAIN)-demo
	cd terraform/demo && terraform destroy -var domain=$(DOMAIN) -var domain_root=$(DOMAIN_ROOT)

sliderule-docker: ## build sliderule docker image; needs VERSION
	-rm -Rf $(SERVER_STAGE_DIR)
	mkdir -p $(SERVER_STAGE_DIR)/etc
	cp -R $(SLIDERULE_SOURCE_DIR) $(SERVER_STAGE_DIR)
	cp docker/sliderule/Dockerfile $(SERVER_STAGE_DIR)
	cp docker/sliderule/*.* $(SERVER_STAGE_DIR)/etc
	cd $(SERVER_STAGE_DIR); docker build $(DOCKEROPTS) -t $(REPO)/sliderule:latest .
	docker tag $(REPO)/sliderule:latest $(REPO)/sliderule:$(VERSION)
	docker tag $(REPO)/sliderule:latest $(REPO)/sliderule:$(MAJOR_VERSION)

monitor-docker: ## build monitor docker image; needs VERSION
	-rm -Rf $(MONITOR_STAGE_DIR)
	mkdir -p $(MONITOR_STAGE_DIR)
	cp docker/monitor/* $(MONITOR_STAGE_DIR)
	cp docker/monitor/Dockerfile.$(ARCH) $(MONITOR_STAGE_DIR)/Dockerfile
	chmod +x $(MONITOR_STAGE_DIR)/docker-entrypoint.sh
	cd $(MONITOR_STAGE_DIR); docker build $(DOCKEROPTS) -t $(REPO)/monitor:latest .
	docker tag $(REPO)/monitor:latest $(REPO)/monitor:$(VERSION)
	docker tag $(REPO)/monitor:latest $(REPO)/monitor:$(MAJOR_VERSION)

ilb-docker: ## build intelligent load balancer docker image; needs VERSION
	-rm -Rf $(ILB_STAGE_DIR)
	mkdir -p $(ILB_STAGE_DIR)
	cp docker/intelligent-load-balancer/* $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/targets/orchestrator-haproxy/orchestrator.lua $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/scripts/extensions/json.lua $(ILB_STAGE_DIR)
	cp $(SLIDERULE_SOURCE_DIR)/scripts/extensions/prettyprint.lua $(ILB_STAGE_DIR)
	cd $(ILB_STAGE_DIR); docker build $(DOCKEROPTS) -t $(REPO)/ilb:latest .
	docker tag $(REPO)/ilb:latest $(REPO)/ilb:$(VERSION)
	docker tag $(REPO)/ilb:latest $(REPO)/ilb:$(MAJOR_VERSION)

ilb-run: ## run intelligent load balancer docker container; needs VERSION
	docker run -it --rm --name=ilb --net=host -v $(ROOT)/test/cluster:/haproxy -e CLUSTER=sliderule -e DOMAIN=localhost $(REPO)/ilb:$(VERSION)

cluster-build-packer: ## build Amazon Machine Image (AMI) for release; needs VERSION
	cd packer; packer build -var version=$(MAJOR_VERSION) sliderule-base.pkr.hcl

cluster-upload-terraform: ## upload the Cluster terraform for provisioning system; needs VERSION
	-rm -Rf $(TF_STAGE_DIR)
	mkdir -p $(TF_STAGE_DIR)
	cp terraform/cluster/*.* $(TF_STAGE_DIR)
	cp terraform/cluster/.terraform.lock.hcl $(TF_STAGE_DIR)
	echo "variable \"environment_version\" {\n  default = \"$(ENVVER)\"\n}" > $(TF_STAGE_DIR)/version.tf
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(VERSION)
	cd $(TF_STAGE_DIR) && aws s3 sync --delete . s3://sliderule/prov-sys/cluster_tf_versions/$(MAJOR_VERSION)

cluster-release: ## release a version of the cluster; needs VERSION
	# release repositories
	cd $(SLIDERULE_SOURCE_DIR); ./RELEASE.sh $(VERSION); git push --tags; git push
	cd $(PYTHON_SOURCE_DIR); ./RELEASE.sh $(VERSION); git push --tags; git push
	cd $(STATIC_WEB_SOURCE_DIR); ./RELEASE.sh $(VERSION); git push --tags; git push
	gh auth login
	cd $(SLIDERULE_SOURCE_DIR); gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/rtd/release_notes/release_notes.html"
	cd $(PYTHON_SOURCE_DIR); gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/rtd/release_notes/release_notes.html"
	cd $(STATIC_WEB_SOURCE_DIR); gh release create $(VERSION) -t $(VERSION) --notes "see https://slideruleearth.io/rtd/release_notes/release_notes.html"
	# docker images
	make DOCKEROPTS=--no-cache sliderule-docker
	make DOCKEROPTS=--no-cache monitor-docker
	make DOCKEROPTS=--no-cache ilb-docker
	make DOCKEROPTS=--no-cache static-website-docker
	make DOCKEROPTS=--no-cache demo-docker
	# docker tags
	docker push $(REPO)/sliderule:$(VERSION)
	docker push $(REPO)/sliderule:$(MAJOR_VERSION)
	docker push $(REPO)/monitor:$(VERSION)
	docker push $(REPO)/monitor:$(MAJOR_VERSION)
	docker push $(REPO)/ilb:$(VERSION)
	docker push $(REPO)/ilb:$(MAJOR_VERSION)
	docker push $(REPO)/static-website:$(VERSION)
	docker push $(REPO)/demo-client:$(VERSION)
	docker push $(REPO)/demo-nginx:$(VERSION)
	# deployment resources
	make cluster-build-packer
	make cluster-upload-terraform

cluster-deploy: ## manually deploy a cluster; needs VERSION, DOMAIN
	cd terraform/cluster && terraform apply \
		-var cluster_name=sliderule \
		-var ami_name=sliderule-$(MAJOR_VERSION) \
		-var sliderule_image=$(REPO)/sliderule:$(MAJOR_VERSION) \
		-var ilb_image=$(REPO)/ilb:$(MAJOR_VERSION) \
		-var monitor_image=$(REPO)/monitor:$(MAJOR_VERSION) \
		-var domain=$(DOMAIN)

cluster-destroy: ## manually destroy a cluster
	cd terraform/cluster && terraform destroy

cluster-docker-clean: ## clean out old version of docker images; needs VERSION
	- docker image rm $(REPO)/sliderule:$(VERSION)
	- docker image rm $(REPO)/ilb:$(VERSION)
	- docker image rm $(REPO)/monitor:$(VERSION)
	- docker image rm $(REPO)/static-website:$(VERSION)
	- docker image rm $(REPO)/demo-client:$(VERSION)
	- docker image rm $(REPO)/demo-nginx:$(VERSION)

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)

help: ## That's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)