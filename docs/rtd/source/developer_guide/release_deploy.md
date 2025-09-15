# Release and Deploy

2025-07-17

## Summary

This guide provides step by step instructions for releasing and deploying a new version of SlideRule to the public cluster.  All steps are performed via Makefile targets running on an EC2 instance provisioned in the SlideRule AWS environment.  The EC2 instance must be setup with access to the SlideRule S3 buckets, container registries, and Git Hub repositories.

SlideRule uses a Blue-Green deployment strategy where the new release is first deployed as a hidden cluster to "sliderule-<color>.slideruleearth.io" and put through a series of tests. Once all tests have passed, the "sliderule.slideruleearth.io" domain is updated to point to the cluster, thereby switching it to become the live cluster.  The previously deployed cluster is no hidden and is left running until the time-to-live on the DNS entries expire and all processing requests have completed.  This should take at most 15 minutes (TTL of 5 minutes + Maximum Request Time of 10 minutes).  The old (and now hidden) cluster is then destroyed.

## Checklist:

* Create (and check in) release notes for the new release by reviewing all commits, PRs, and GitHub issues. (See __sliderule/docs/rtd/source/developer_guide/release_notes__).

* Make sure there are no uncommitted changes and the local `main` branch is in sync with the remote repository.
```bash
git checkout main
git status
git pull
```

* Update .aws/credentials file with a temporary session token; {profile} references your long term aws credentials, {username} is your aws account, {code} is your MFA token.  This is necessary for local development and tests to access things like the sliderule S3 bucket.  It is also necessary as a precursor to logging into the container registry.
```bash
aws --profile={profile} sts get-session-token --serial-number arn:aws:iam::742127912612:mfa/{username} --token-code={code}
```

* Login to AWS Elastic Container Registry.  A part of the deployment process is pushing the recently built and tagged container images to the registry so that they can be pulled in the production system.
```bash
aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
```

* Login to GitHub.  GitHub has the concept of a "release" which is a construct on top of a repository tag and includes release artifacts and its own description.  The GitHub release is performed from the command line using the `gh` tool.
```bash
gh auth login
```

* Select version of `npm`.  If the Node Version Manager tool is used, then a compatible version of node must be selected and active in the environment being used for the release.
```bash
nvm use 20
```

* Login to NPMJS.  Each release includes publishing to the public npm registry.
```bash
cd sliderule/clients/nodejs && make login
```

* Verify the presence of the conda environments needed to run the different pytest suites: `sliderule`, `manager`, `ams`.
```bash
conda env create -f clients/python/environment.yml # creates sliderule environment
conda env create -f targets/slideruleearth/docker/manager/environment.yml # creates manager environment
conda env create -f applications/ams/environment.yml # creates ams environment
conda env list # lists what conda environments are available
```
## Guide

#### (1) Run Local Self-Tests

* Compile the code in debug mode, which runs through a slew of static analysis using the `clang tidy` and `cppcheck` tools.

* Run the `Lua` selftest, which executes all of the component level test scripts with `address sanitizer` enabled.

From `sliderule/targets/slideruleearth`
```bash
make distclean
make config-debug
make
make selftest
```

* Run the `manager` PyTests. The manager application has its own set of pytests that run under the `manager` conda environment and test the basic functionality and APIs of the *manager* container.

From `sliderule/targets/slideruleearth/docker/manager`
```bash
make test
```

* Run the `ams` PyTests. The Asset Metadata Service (AMS) application has its own set of pytests that run under the `ams` conda environment and test the basic functionality and APIs of the *ams* container.

From `sliderule/applications/ams`
```bash
make test
```

#### (2) Test Local Deployment (Pytests and Jest)

* Run a debug version of the sliderule server code locally. Make sure the `ilb`, `manager`, and `ams` containers are running in the background.

From `sliderule/targets/slideruleearth`
```bash
docker compose up ilb manager ams -d
```

From `sliderule/targets/slideruleearth`
```bash
make distclean
make config-debug
make
make run
```

* Run the pytest suite of tests against it, which checks for memory leaks and invalid memory access across the entire pytest suite.  Note that tests involving the provisioning system will fail when running against a locally running instance of sliderule.

From `sliderule/clients/python` in a new terminal window
```bash
conda activate sliderule
pip install .
pytest --domain localhost --organization None
```

* Run the Node.js client suite of tests using `jest`, which test the client and also try to test some of the functionality needed by the web client.  Note that the authentication tests will fail when running against a locally running instance of sliderule.

From `sliderule/clients/nodejs`
```bash
nvm use 20
ORGANIZATION=null make test
```

#### (3) Test Deployment to Private Cluster

Prior to a full release, it is good to test the full deployment to a private cluster.  This catches issues related to the infrastructure in AWS and errors that don't surface in a local environment but do surface when run as a cluster in the cloud.  It is recommended to use the provisioning system to deploy the private cluster (https://ps.slideruleearth.io), as that gives good insight to when the cluster is deployed and if there are any issues in the deployment.  But the cluster can also be deployed using the `--desired_nodes` and `--time_to_live` parameters of the pytests, see below for where to use them.

* Build the latest code base and deploy it to a private cluster (typically `developers`).

From `sliderule/targets/slideruleearth`
```bash
make cluster-docker
make cluster-docker-push
make cluster-update-terraform
```

* Execute the pytest tests against the deployed private cluster.

From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
pytest --organization developers # --desired_nodes 7 --time_to_live 120
```

* Execute the jest tests against the deployed private cluster.

From `sliderule/clients/nodejs`
```bash
nvm use 20
ORGANIZATION=developers make test
```

* Run benchmarks and baseline

While the latest private cluster is still deployed, run the benchmarks and baseline scripts against it.  The private cluster should be running 7 nodes.
From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
python utils/benchmarks.py --organization developers
python utils/baseline.py --organization developers
```

* Update the release notes and make sure all changes are committed and pushed.  Other than the release notes, there should be no new changes in the repository at this point; if there are, then the steps above need to be rerun.

#### (4) Create New Release

From `sliderule/targets/slideruleearth`
```bash
make release VERSION=<version>
```
where `version` is in the form **vX.Y.Z**.

This directly performs at least the following:
* Builds the latest sliderule docker build environment `sliderule-buildenv`
* Updates the build dependencies in the `libdep.lock` file
* Updates the `manager` lock file
* Updates the `ams` lock file
* Tags the repository
* Creates a GitHub release
* Builds and pushes/uploads the cluster (docker containers, AMI, terraform)
* Builds and pushes/uploads the static website

This indirectly kicks off a series of GitHub actions:
* Publishes the updated PyPI package for the SlideRule Python Client
* Creates a Pull Request in the conda-forge feedstock for the SlideRule Python Client
* Builds and runs the self-tests
* Deployes the code, using the `unstable` tag, to the `cicd` cluster, and runs the pytest suite against it

Note - when a new release is created, there is a GitHub action that automatically creates and publishes a PyPI package.  Once published, the Conda Forge feedstock for the SlideRule Python Client package automatically kicks off the process of building and releasing the updated conda-forge package.  Once the package is built, the final step of merging the PR is manual and must be done by going to: https://github.com/conda-forge/sliderule-feedstock

#### (5) Deploy the Newly Released Version to the Public Cluster

* Check which color the live cluster is running as. Only one of the colors should have actively deployed resources in AWS.  Consider the active color as the live cluster, and use the inactive color for the new cluster.

From `sliderule/targets/slideruleearth/`
```bash
make public-cluster-status COLOR=green
make public-cluster-status COLOR=blue
```

* Deploy the new release as the _new_ cluster color. The `<new>` is the _new_ color identified in the step above.  The `<new release version>` is the version of the code that is to be deployed.

From `sliderule/targets/slideruleearth/`
```bash
make public-cluster-deploy COLOR=<new> VERSION=<new release version>
```

* Test out the _new_ cluster, using `sliderule-<new>` as the organization.

From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
pytest --organization sliderule-<new>
```

From `sliderule/clients/nodejs`
```bash
nvm use 20
ORGANIZATION=sliderule-<new> make test
```

* Switch to the _new_ cluster by pointing the `sliderule.slideruleearth.io` domain to the newly deployed cluster.  The `<ip>` address supplied in the command is the `ilb_ip_address` output from the deployment of the _new_ cluster.

From `sliderule/targets/slideruleearth/`
```bash
make public-cluster-go-live PUBLIC_IP=<ip>
```

* Wait 30 minutes. This will allow DNS caches to flush and any active requests on the old cluster to complete.

* Run the usage report and save off the previous clusters database.  The `--export` option saves the database to the SlideRule S3 bucket at `s3://sliderule/data/manager`.
```bash
python utils/usage_report.py --organization sliderule-<color> --export --apikey <key>
python utils/usage_report.py --organization sliderule-<color>
```

* Destroy the old cluster. The `<old>` color is the color of the old cluster that we want to now destroy.

from `sliderule/targets/slideruleearth/`
```bash
make public-cluster-destroy COLOR=<old>
```