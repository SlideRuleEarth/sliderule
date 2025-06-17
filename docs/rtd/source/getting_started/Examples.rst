========
Examples
========

The following Jupyter notebooks provide examples of how to use some of SlideRule's functionality.
They are listed roughly in the order of complexity, with the simpler examples first and the more complex examples farther down.
The source code for all of these notebooks can be found in our `repository <https://github.com/SlideRuleEarth/sliderule/clients/python/examples>`_.

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Examples

   ../assets/boulder_watershed
   ../assets/grandmesa
   ../assets/phoreal
   ../assets/arcticdem_mosaic
   ../assets/grandmesa_atl03_classification
   ../assets/atl13_access
   ../assets/atl24_access

Additional files are necessary to run some of the notebooks locally.  They are:

 * :download:`grandmesa.geojson <../assets/grandmesa.geojson>`
 * :download:`dicksonfjord.geojson <../assets/dicksonfjord.geojson>`

Notebooks
##################

- Boulder Watershed (:download:`download <../assets/boulder_watershed.ipynb>`)
   A simple notebook to demonstrate a basic ``atl03x`` processing request.  Elevation data is generated for the Boulder watershed region and plotted using matplotlib.

- Grand Mesa (:download:`download <../assets/grandmesa.ipynb>`)
   Demonstrates how to request custom ATL06 elevations from SlideRule for a region of interest, and then use SlideRule APIs to read and compare the results to the ATL06 standard data product.

- PhoREAL (:download:`download <../assets/phoreal.ipynb>`)
   Demonstrate use of the PhoREAL algorithm running inside SlideRule.  Vegetation metrics are calculated over the Grand Mesa region and then later combined with calculated elevations.

- ArcticDEM Mosaic (:download:`download <../assets/arcticdem_mosaic.ipynb>`)
   Demonstrates how to sample the ArcticDEM Mosaic raster at generated ATL06-SR points and return all of the data as a unified GeoDataFrame.

- ATL03 Classification (:download:`download <../assets/grandmesa_atl03_classification.ipynb>`)
   An in-depth example of requesting ATL03 photon data classified using ATL08 and YAPC.  The results are plotted using matplotlib.

- ATL13 (:download:`download <../assets/atl13_access.ipynb>`)
   Demonstrates different ways to access the ATL13 inland lake data: by reference ID, by name, and by contained coordinate.

- ATL24 (:download:`download <../assets/atl24_access.ipynb>`)
   Subsets ATL24 near-shore bathymetry data using different methods and parameters.