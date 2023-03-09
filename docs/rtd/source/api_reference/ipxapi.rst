======
ipxapi
======

The icepyx Python API ``ipxapi.py`` is used to access the services provided by the **sliderule-icesat2** plugin for SlideRule using the ``icepyx`` library.
It mirrors functions provided in the ``icesat2.py`` module, and provides a simplified interface that accepts icepyx Query objects (regions).

The module can be imported via:

.. code-block:: python

    from sliderule import ipxapi

For more information about icepyx, go to `icepyx GitHub <https://github.com/icesat2py/icepyx>`_ or `icepyx ReadTheDocs <https://icepyx.readthedocs.io/en/latest/>`_.


""""""""""""""""

atl06p
------
.. autofunction:: sliderule.ipxapi.atl06p

""""""""""""""""

atl03sp
-------
.. autofunction:: sliderule.ipxapi.atl03sp


to_region
---------
.. autofunction:: sliderule.ipxapi.to_region
