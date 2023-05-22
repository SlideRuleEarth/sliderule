ROOT = $(shell pwd)/../..
TEST ?=

test: ## run unit tests against public cluster
	jest $(TEST)

test-local: ## run unit tests against local running sliderule server
	jest $(TEST) --setupFiles $(ROOT)/local-config.json

test-developer: ## run unit tests against developers cluster
	jest $(TEST) --setupFiles $(ROOT)/developer-config.json

install: ## install project (installs dependencies in package.json file)
	npm install

init: ## initialize project
	npm init

distclean: ## fully remove all non-version controlled files and directories
	-rm -Rf node_modules/

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
	@echo ENVVER:$(ENVVER)