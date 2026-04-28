========
Examples
========

The following Jupyter notebooks provide examples of how to use some of SlideRule's functionality.
They are listed roughly in the order of complexity, with the simpler examples first and the more complex examples farther down.
The source code for all of these notebooks as well as additional notebooks can be found in our `repository <https://github.com/SlideRuleEarth/sliderule/tree/main/clients/python/examples>`_.

Additional files are necessary to run some of the notebooks locally.

 * :download:`grandmesa.geojson </_static/grandmesa.geojson>`
 * :download:`dicksonfjord.geojson </_static/dicksonfjord.geojson>`

Notebooks

- `Boulder Watershed <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/boulder_watershed.ipynb>`_ (:download:`download </_static/boulder_watershed.ipynb>`)
   A simple notebook to demonstrate a basic ``atl03x`` processing request.  Elevation data is generated for the Boulder watershed region and plotted using matplotlib.

- `Grand Mesa <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/grandmesa.ipynb>`_ (:download:`download </_static/grandmesa.ipynb>`)
   Demonstrates how to request custom ATL06 elevations from SlideRule for a region of interest, and then use SlideRule APIs to read and compare the results to the ATL06 standard data product.

- `PhoREAL <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/phoreal.ipynb>`_ (:download:`download </_static/phoreal.ipynb>`)
   Demonstrate use of the PhoREAL algorithm running inside SlideRule.  Vegetation metrics are calculated over the Grand Mesa region and then later combined with calculated elevations.

- `ArcticDEM Mosaic <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/arcticdem_mosaic.ipynb>`_ (:download:`download </_static/arcticdem_mosaic.ipynb>`)
   Demonstrates how to sample the ArcticDEM Mosaic raster at generated ATL06-SR points and return all of the data as a unified GeoDataFrame.

- `ATL03 Classification <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/grandmesa_atl03_classification.ipynb>`_ (:download:`download </_static/grandmesa_atl03_classification.ipynb>`)
   An in-depth example of requesting ATL03 photon data classified using ATL08 and YAPC.  The results are plotted using matplotlib.

- `ATL13 <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/atl13_access.ipynb>`_ (:download:`download </_static/atl13_access.ipynb>`)
   Demonstrates different ways to access the ATL13 inland lake data: by reference ID, by name, and by contained coordinate.

- `ATL24 <https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/atl24_access.ipynb>`_ (:download:`download </_static/atl24_access.ipynb>`)
   Subsets ATL24 near-shore bathymetry data using different methods and parameters.