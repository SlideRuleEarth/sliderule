import os
from setuptools import setup, find_packages

# get long_description from README.md
with open("README.md", "r") as fh:
    long_description = fh.read()

# get install requirements
with open('requirements.txt') as fh:
    install_requires = fh.read().splitlines()

# get version
with open('version.txt') as fh:
    version = fh.read().strip()
    if version[0] == 'v':
        version = version[1:]

# list of all utility scripts to be included with package
scripts=[os.path.join('utils',f) for f in os.listdir('utils') if f.endswith('.py')]

setup(
    name='sliderule',
    author='SlideRule Developers',
    description='Python client for interacting with sliderule server',
    long_description_content_type="text/markdown",
    url='https://github.com/ICESat2-SlideRule/sliderule-python/',
    license='BSD 3-Clause',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Science/Research',
        'Topic :: Scientific/Engineering :: Physics',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
    ],
    packages=find_packages(),
    version=version,
    install_requires=install_requires,
    scripts=scripts,
)
