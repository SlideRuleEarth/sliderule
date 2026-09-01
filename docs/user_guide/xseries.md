# X-Series APIs

:::{note}
This page documents the `x-series` APIs that are specifically geared for generating and processing DataFrames.  These APIs were made public in early 2025 starting with version 4.11.0, and have a common methodology for processing the data which makes interfacing to them consistent across multiple datasets.  Much of the functionality described here is duplicated in older-style `p-series` and `s-series` APIs described elsewhere.  While the older-style APIs will continue to be supported, most of the new development effort is focused on these new DataFrame centric APIs.
:::

DataFrame APIs are accessed via the following ***sliderule*** function and always produce a __GeoDataFrame__:
```python
gdf = sliderule.run(api, parms, aoi=None, resources=None)
```
where:
* `api` is a string that specifies which SlideRule API is being called
* `parms` is the request parameter dictionary
* `aoi` is the polygon defining the area of interest (provided for convenience only; if not supplied, the area of interest is taken directly from __parms__)
* `resources` is the list of granules to process (provided for convenience only; if not supplied, the resources are taken directly from __parms__)

The columns in the returned __GeoDataFrame__ depend on the contents of the `parms` structure passed to the `api`.  Typically, there are a base set of columns defined for each `api`.  If __algorithms__ are specified in the `parms`, then the columns may be completely different depending upon the output of the __algorithms__.  If __ancillary fields__ or __raster samplers__ are specified, then there may be additional columns.

Metadata returned with the results of the API is placed in the `attrs` attribute of the __GeoDataFrame__.  There are three groups of metadata:
* `meta`: the metadata provided in the server-side dataframe; typically includes the `endpoint`, and the `srctbl` which correlates the `srcid` column values to source granules
* `sliderule`: the server-side parameters used in processing the request along with versioning information
* `recordinfo`: identifies special columns in the dataframe for generalized processing (e.g. time, x, y, z)

Here is an example for accessing the source granule of an entry in the dataframe with a given `srcid`.
```python
gdf = sliderule.run(api, parms)
gdf.attrs['meta']['srctbl'][f'{srcid}']
```
