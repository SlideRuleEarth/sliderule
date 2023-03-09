# Publishing Python Packages to PyPI

2021-07-15

## Overview

How to build and publish sliderule-python for PyPI.

## Steps

The ___sliderule___ PyPI package is located at https://pypi.org/project/sliderule/.

### 1. Clear Out Existing Outputs

Run the `./clean.sh` script in the base directory of the ___sliderule-python___ repository to remove all existing outputs.  Later steps may inadvertantly use them.

### 2. Build Binary Distribution

Run `python setup.py bdist` in the base directory of the ___sliderule-python___ repository.  This will use the `version.txt` file to grab the version of the package it builds.

### 3. Upload to PyPI

First, make sure you have ___twine___ installed. If not, then from the desired Python environment, run `pip install twine`.

Then, run `twine upload dist/*` from the same location you ran ___setup.py___.

This will upload the versioned package to PyPI.  NOTE: if the version already exists, then the upload will fail.  So make sure this is only performed on releases.
