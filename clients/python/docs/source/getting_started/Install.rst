============
Installation
============

Presently `sliderule-python` is only available for use as a `GitHub repository <https://github.com/ICESat2-SlideRule/sliderule-python>`_.
The contents of the repository can be download as a `zipped file <https://github.com/ICESat2-SlideRule/sliderule-python/archive/main.zip>`_  or cloned.

To use this repository, please fork into your own account and then clone onto your system:

.. code-block:: bash

    git clone https://github.com/ICESat2-SlideRule/sliderule-python.git

Can then install using `setuptools`:

.. code-block:: bash

    python3 setup.py install

.. code-block:: bash

    python3 -m pip install --user .

Alternatively can install the `sliderule` utilities directly from GitHub with `pip`:

.. code-block:: bash

    python3 -m pip install --user git+https://github.com/ICESat2-SlideRule/sliderule-python.git

For developer installs, you can use the provided environment file to create an initial conda environment that has sliderule installed:

.. code-block:: bash

    conda env create -f environment.yml

Executable versions of this repository can also be tested using
`Binder <https://mybinder.org/v2/gh/ICESat2-SlideRule/sliderule-python/main>`_ or
`Pangeo <https://binder.pangeo.io/v2/gh/ICESat2-SlideRule/sliderule-python/main>`_.
