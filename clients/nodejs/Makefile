ROOT = $(shell pwd)
TEST ?= tests
DOMAIN ?= slideruleearth.io
ORGANIZATION ?= sliderule

test: ## run unit tests against public cluster
	cd sliderule; DOMAIN=$(DOMAIN) ORGANIZATION=$(ORGANIZATION) jest $(TEST) --setupFiles $(ROOT)/sliderule/tests/test.config.js

test-ci:
	DOMAIN=$(DOMAIN) ORGANIZATION=$(ORGANIZATION) ./node_modules/.bin/jest $(ROOT)/sliderule/$(TEST) --setupFiles $(ROOT)/sliderule/tests/test.config.js

publish: ## publish package to npm registry
	cd sliderule; npm publish --access public

login: ## login to npm registry
	npm login --scope sliderule --auth-type legacy

update: ## update project (updates dependencies in package-lock.json file)
	cd sliderule; npm install
	npx playwright install

init: ## initialize project
	cd sliderule; npm init --scope sliderule

distclean: ## fully remove all non-version controlled files and directories
	-rm -Rf sliderule/node_modules/

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ROOT:$(ROOT)