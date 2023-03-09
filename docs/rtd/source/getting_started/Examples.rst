.. _examples:

========
Examples
========

The following Jupyter notebooks provide examples of how to use some of SlideRule's functionality.  They are listed roughly in the order of complexity, with the simpler examples first and the more complex examples farther down.  Each example includes a few different links for you to view and use the example:

*html*
  Rendered notebook with all output cells included; typically includes verbose option so that as much information is available as possible.
*nbviewer*
  Rendered notebook with no output cells, just source cells; uses the notebook as it exists on the ``main`` branch in our development git repository.
*download*
  Source notebook in the `ipynb` format for you to download and use.

Additional files are necessary to run some of the notebooks locally.  They are:

 * :download:`grandmesa.geojson <../assets/grandmesa.geojson>`
 * :download:`dicksonfjord.geojson <../assets/dicksonfjord.geojson>`


Example Notebooks
#################

- Boulder Watershed Example (`html </rtd/_static/html/boulder_watershed_demo.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/boulder_watershed_demo.ipynb>`_ | :download:`download <../assets/boulder_watershed_demo.ipynb>`)
    A simple notebook to demonstrate a basic ``atl06`` processing request.  Elevation data is generated for the Boulder watershed region and plotted using matplotlib.

- Grand Mesa Demo (`html </rtd/_static/html/grand_mesa_demo.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/grand_mesa_demo.ipynb>`_ | :download:`download <../assets/grand_mesa_demo.ipynb>`)
    Demonstrates how to request custom ATL06 elevations from SlideRule for a region of interest, and then use SlideRule APIs to read and compare the results to the ATL06 standard data product.

- Single Track Demo (`html </rtd/_static/html/single_track_demo.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/single_track_demo.ipynb>`_ | :download:`download <../assets/single_track_demo.ipynb>`)
    Demonstrates how to request custom ATL06 elevations from SlideRule for a single ICESat-2 granule and then use SlideRule APIs to read and compare the results to the ATL06 standard data product.

- ArcticDEM Mosaic Example (`html </rtd/_static/html/arcticdem_mosaic.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/arcticdem_mosaic.ipynb>`_ | :download:`download <../assets/arcticdem_mosaic.ipynb>`)
    Demonstrates how to sample the ArcticDEM Mosaic raster at generated ATL06-SR points and return all of the data as a unified GeoDataFrame.

- ArcticDEM Strips Example (`html </rtd/_static/html/arcticdem_strip_boundaries.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/arcticdem_strip_boundaries.ipynb>`_ | :download:`download <../assets/arcticdem_strip_boundaries.ipynb>`)
    Demonstrates how to sample the ArcticDEM Strip rasters at generated ATL06-SR points and return all of the data as a unified GeoDataFrame.  The notebook also includes code to pull out the bounding box of each source strip and plot it against the region of interest and ATL06-SR data points.

- ATL03 Classification Example (`html </rtd/_static/html/grand_mesa_atl03_classification.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/grand_mesa_atl03_classification.ipynb>`_ | :download:`download <../assets/grand_mesa_atl03_classification.ipynb>`)
    An in-depth example of requesting ATL03 photon data classified using ATL08 and YAPC.  The results are plotted using matplotlib.

- ATL03 Subsetting using IPython Widgets (`html </rtd/_static/html/atl03_widgets_demo.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/atl03_widgets_demo.ipynb>`_ | :download:`download <../assets/atl03_widgets_demo.ipynb>`)
    Demonstrates how to read ATL03 photon data using the `ipysliderule` module to specify the region of interest and parameters for the request.  The photon data is plotted on a leaflet map, as well as along track using matplotlib.

- IPython Widgets Example (`html </rtd/_static/html/api_widgets_demo.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/api_widgets_demo.ipynb>`_ | :download:`download <../assets/api_widgets_demo.ipynb>`)
    Demonstrates common uses of the `ipysliderule` module, which provides Jupyter widgets to set parameters for SlideRule API requests and display results from SlideRule API responses.

- CMR Region Check Tool (`html </rtd/_static/html/cmr_debug_regions.html>`_ | `nbviewer <https://nbviewer.org/github/ICESat2-SlideRule/sliderule-python/blob/main/examples/cmr_debug_regions.ipynb>`_ | :download:`download <../assets/cmr_debug_regions.ipynb>`)
    Demonstration of sophisticated techniques for using SlideRule and IPython widgets to visualize and analyze the results returned from ICESat-2 CMR queries.
