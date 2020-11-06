import os
from setuptools import setup, find_packages

install_requires = ['numpy', 'sliderule']

setup(
    name='icesat2',
    author='ICESat-2/SlideRule Developers',
    version='0.2.0',
    description='An ICESat-2 plugin for SlideRule.',
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
