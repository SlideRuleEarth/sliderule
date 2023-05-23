ROOT = $(shell pwd)
TEST ?= tests

test: ## run unit tests against public cluster
	cd sliderule; jest $(TEST)

test-local: ## run unit tests against local running sliderule server
	cd sliderule; jest $(TEST) --setupFiles $(ROOT)/sliderule/tests/local.config.js

test-developer: ## run unit tests against developers cluster
	cd sliderule; jest $(TEST) --setupFiles $(ROOT)/sliderule/tests/developer.config.js

publish: ## publish package to npm registry
	cd sliderule; npm publish --access public

login: ## login to npm registry
	npm login --scope sliderule --auth-type legacy

install: ## install project (installs dependencies in package.json file)
	cd sliderule; npm install

init: ## initialize project
	cd sliderule; npm init --scope sliderule

distclean: ## fully remove all non-version controlled files and directories
	-rm -Rf sliderule/node_modules/

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)