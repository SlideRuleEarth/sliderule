# DataFrames

SlideRule includes a set of APIs that are specifically geared for generating and processing DataFrames.  These APIs were made public in early 2025 starting with version 4.11.0, and have a common methodology for processing the data which makes interfacing to them consistent across multiple datasets.

DataFrame APIs are accessed via the following ***sliderule*** function which always returns a __GeoDataFrame__:
```python
sliderule.run(api, parms, aoi=None, resources=None)
```
where:
* `api` is a string that specifies which SlideRule API is being called
* `parms` is the request parameter dictionary
* `aoi` is the polygon defining the area of interest (provided for convenience only; if not supplied, the area of interest is taken directly from __parms__)
* `resources` is the list of granules to process (provided for convenience only; if not supplied, the resources are taken directly from __parms__)

## APIs

The following APIs support the DataFrame interface.

### atl03x

This API is used to subset and operate on the ICESat-2 ATL03 photon cloud data and is called via:
```python
sliderule.run('atl03x', parms)
```
See the [ICESat-2 documentation](/web/rtd/user_guide/icesat2.html) for a description of the parameters.

The resulting DataFrame contains the following:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds||

### atl24x

This API is used to subset and operate on the ICESat-2 ATL24 bathymetry classified photons and is called via:
```python
sliderule.run('atl24x', parms)
```

## Runners

### Sampler

### Surface Fitter

### PhoREAL