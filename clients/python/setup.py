import os
from setuptools import setup, find_packages

# get long_description from README.md
with open("README.md", "r") as fh:
    long_description = fh.read()

# get install requirements
with open('requirements.txt') as fh:
    install_requires = fh.read().splitlines()

setup(
    name='sliderule',
    author='SlideRule Developers',
    version='0.2.0',
    description='A framework for on-demand ICESat-2 data processing.',
    long_description_content_type="text/markdown",
    url='https://github.com/ICESat2-SlideRule/sliderule/',
    license='Apache',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Science/Research',
        'Topic :: Scientific/Engineering :: Physics',
        'License :: OSI Approved :: Apache License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
    ],
    packages=find_packages(),
    install_requires=install_requires,
)
