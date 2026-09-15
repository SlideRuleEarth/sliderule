TEST ?= tests
DOMAIN ?= slideruleearth.io
ORGANIZATION ?= sliderule

test: ## run unit tests against public cluster
	cd sliderule; DOMAIN=$(DOMAIN) ORGANIZATION=$(ORGANIZATION) jest $(TEST) --setupFiles $(ROOT)/sliderule/tests/test.config.js

login: ## login to npm registry

update: ## update project (updates dependencies in package-lock.json file)
	cd sliderule; npm install && npm audit fix

init: ## initialize project
	cd sliderule; npm init --scope sliderule

release: ## release a new version of the package
	npm login
	node utils/modpkg.js $(RELEASE)
	git add sliderule/package.json
	cd sliderule; npm publish --access public

distclean: ## fully remove all non-version controlled files and directories
	-rm -Rf sliderule/node_modules/

help: ## That's me!
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
