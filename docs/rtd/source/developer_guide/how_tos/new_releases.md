# Checklist for New Releases

2025-05-09

## Overview

Provided are a list of steps needed when a new SlideRule release is created and deployed.

## Checklist

#### (1) Create Release Notes

Create (and check in) release notes for the new release by reviewing all commits, PRs, and GitHub issues.
> sliderule/docs/rtd/source/developer_guide/release_notes

#### (2) Confirm Clean Git Repository

Make sure there are no uncommitted changes and the local `main` branch is in sync with the remote repository.
```bash
git checkout main
git status
git pull
```

#### (3) Run Self Test

Compiling the code in debug mode runs through a slew of static analysis.  Running the selftest executes all of the component level test scripts.

From `sliderule/targets/slideruleearth-aws`
```bash
make distclean
make config-debug
make
make selftest
```

#### (4) Build and Deploy Latest Cluster

Build the latest code base and deploy it to a private cluster (typically `developers`).

From `sliderule/targets/slideruleearth-aws`
```bash
make cluster-docker
make cluster-docker-push
make cluster-update-terraform
```

#### (5) Run PyTests

Execute the latest pytest tests against the latest deployed private cluster.

From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
pytest --organization developers
```

If it hasn't been done already during development, the pytest suite should also run against a locally build debug version of the code to check for memory leaks and invalid memory access.

From `sliderule/targets/slideruleearth-aws`
```bash
make distclean
make config-debug
make
make run
```

From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
pytest --domain localhost --organization None
```

#### (6) Run Benchmarks

While the latest private cluster is still deployed, run the benchmarks against it.  The private cluster should be running 7 nodes.
From `sliderule/clients/python`
```bash
conda activate sliderule
pip install .
python utils/benchmarks.py --organization developers
```

Then update the release notes and make sure all changes are committed and pushed.  There should be no new changes in the repository at this point; if there are, then the steps above need to be rerun.

#### (7) Create New Release

From `sliderule/targets/slideruleearth-aws`
```bash
make release VERSION=<version>
```
where `version` is in the form **vX.Y.Z**.

This will go through and perform the following steps:
* Update the `manager` lock file
* Build the latest sliderule docker build environment and capture new dependencies in the `libdep.lock` file
* Tag the repository
* Build and push the cluster
* Build and push the static website

#### (8) Update SlideRule Python Client

The conda-forge feedstock for the SlideRule Python client will automatically generate a PR for the latest version, but merging the PR is manual.

https://github.com/conda-forge/sliderule-feedstock