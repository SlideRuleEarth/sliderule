# Raster Sampling

## Overview

SlideRule supports sampling raster data at points of interest and including those sampled values alongside its customized data products.  For instance, when performing an ATL06-SR processing run (`atl06p`), the returned GeoDataFrame has a row for each calculated elevation; that row can also include values from different raster datasets that have been sampled at the geolocation of the calculated elevation.

:::{note}
Raster data consists of 2-dimensional datasets that form a grid of square pixels, often called an image.  A common format for storing raster data is TIFF.  GeoTIFF is an extension to the TIFF format that embeds geospatial information into the TIFF file that ties the raster data to a geospatial reference.  COGs are cloud-optimized GeoTIFFs that are internally optimized for access in the cloud.  For more information see https://www.cogeo.org.
:::

In order to sample a raster dataset, SlideRule must first ascertain which individual raster files in the dataset intersect the point of interest, then obtain credentials to access the identified files, and then lastly, open up those files and read the necessary pixels to calculate the returned sample value.  Unfortunately, most raster datasets are organized slightly differently and require a small amount of specialized code to perform the first step of determining which raster files need to be sampled.  The second step of obtaining credentials also requires some specialized code, but since most of our datasets are in AWS and authenticated through NASA DAACs, most of the authentication code is generic.  But even still, because of this, each raster dataset supported by SlideRule needs to be registered with SlideRule ahead of time and provided in what we call an Asset Directory.

## Parameters

To request raster sampling, the ``"samples"`` parameter must be populated as a dictionary in the request.  Each key in the dictionary is used to label the data returned for that raster in the returned DataFrame.

* ``"samples"``: dictionary of rasters to sample
    - ``"<key>"``: user supplied name used to identify results returned from sampling this raster
        - ``"asset"``: name of the raster (as supplied in the Asset Directory) to be sampled
        - ``"algorithm"``: algorithm to use to sample the raster; the available algorithms for sampling rasters are: NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, Gauss
        - ``"radius"``: the size of the kernel in meters when sampling a raster; the size of the region in meters for zonal statistics
        - ``"zonal_stats"``: boolean whether to calculate and return zonal statistics for the region around the location being sampled
        - ``"with_flags"``: boolean whether to include auxiliary information about the sampled pixel in the form of a 32-bit flag
        - ``"t0"``: start time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"t1"``: stop time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"substr"``: substring filter for rasters to be sampled; the raster will only be sampled if the name of the raster includes the provided substring (useful for datasets that have multiple rasters for a given geolocation to be sampled)
        - ``"closest_time"``: time used to filter rasters to be sampled; only the raster that is closest in time to the provided time will be sampled - can be multiple rasters if they all share the same time (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"use_poi_time"``: overrides the "closest_time" setting (or provides one if not set) with the time associated with the point of interest being sampled
        - ``"catalog"``: geojson formatted stac query response (obtained through the `sliderule.earthdata.stac` Python API)
        - ``"bands"``: list of bands to read out of the raster, or a predefined algorithm that combines bands for a given dataset
        - ``"key_space"``: 64-bit integer defining the upper 32-bits of the ``file_id``; this in general should never be set as the server will typically do the right thing assigning a key space;   but for users that are parallelizing requests on the client-side, this parameter can be usedful when constructing the resulting file dictionaries that come back with the raster samples

```Python
    parms {
        "samples" : {
            "mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True},
            "strips": {"asset": "arcticdem-strips", "algorithm": "CubicSpline"}
        }
    }
```

See the [asset directory](https://github.com/SlideRuleEarth/sliderule/blob/main/targets/slideruleearth-aws/asset_directory.csv) for details on which rasters can be sampled.

Sampling rasters is controlled by the `samples` field of a request's parameter dictionary.  The `samples` field is itself a dictionary of dictionaries, each describing a raster dataset that needs to be sampled for the given processing request.  The example below specifies two raster datasets that need to be sampled:

The `mosaic` and `strips` dictionary keys are provided by the user to identify the data returned back to the user in the GeoDataFrame - they can be anything the user wants as long as it unique.  The `asset` dictionary keys are used to identify which asset in SlideRule's asset directory is to be sampled.  The dictionary keys that follow all control how the raster is to be sampled.

SlideRule supports various algorithms for sampling rasters: NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, and Gauss.  These are the algorithms supplied by GDAL and exposed to users through the SlideRule APIs.  In addition to these algorithms, SlideRule also provides the ability to calculate zonal statistics for a given geolocation: count (number of pixels), minimum, maximum, mean, median, standard deviation, and median absolute deviation. Lastly, depending on the dataset, SlideRule provides custom bands that can be derived using other bands in the raster file.  For example, the HLS dataset (`landsat-hls`) provides the following three custom bands: "NDSI", "NDVI", "NDWI".

Prior to sampling a raster, SlideRule provides ways to select and filter which rasters are sampled in a dataset.  These include filtering based on time, and file name pattern matching.

## Sample Results

The output returned in the DataFrame can take two different forms depending on the nature of the data requested.

(1) If the raster being sampled includes on a single value for each latitude and longitude, then the data returned will be of the form {key}.value, {key}.time, {key}.file_id, {key}.{zonal stat} where the zonal stats are only present if requested.

(2) If the raster being sampled has multiple values for a given latitude and longitude (e.g. multiple strips per scene, or multiple bands per image), then the data returned will still have the same column headers, but the values will be numpy arrays.  For a given row in a DataFrame, the length of the numpy array in each column associated with a raster should be the same.  From row to row, those lengths can be different.

The standard columns added to a GeoDataFrame for each sampled raster dataset are:

- __"value"__: the sampled value from the raster at the point of interest
- __"time"__: (for older s-series and p-series APIs) the best time provided by the raster dataset for when the sampled value was measured, returned as GPS seconds
- __"time_ns"__: (for x-series APIs) the best time provided by the raster dataset for when the sampled value was measured, returned as Unix(ns) time
- __"file_id"__: a number used to identify the name of the file the sample value came from; this is used in conjunction with the `file_directory` provided in the metadata of a GeoDataFrame
- __"flags"__: any flags (if requested) that acompany the sampled data in the raster it was read from

The zonal statistic columns added to a GeoDataFrame for each sampled raster dataset are:

- __"count"__: number of pixels read to calculate sample value
- __"min"__: minimum pixel value of pixels that contributed to sample value
- __"max"__: maximum pixel value of pixels that contributed to sample value
- __"mean"__: average/mean pixel value of pixels that contributed to sample value
- __"median"__: average/median pixel value of pixels that contributed to sample value
- __"stdev"__: standard deviation of pixel values of pixels that contributed to sample value
- __"mad"__: median absolute deviation of pixel values of pixels that contributed to sample value

## Providing your own catalog

By default, the SlideRule server code will inspect each request and will automatically make queries to the appropriate catalog for each raster that needs to be sampled. But sometimes the user may wish to have more control over which files are sampled; when that is the case, the user can supply a STAC response directly in their request.

For example, if you wanted to sample all HLS rasters that intersect the following polygon:
```python
polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
            {"lon": -179.0000000001, "lat": 51.0000000001},
            {"lon": -179.0000000001, "lat": 49.0000000001},
            {"lon": -177.0000000001, "lat": 49.0000000001},
            {"lon": -177.0000000001, "lat": 51.0000000001} ]
```
collected in the month of January in year 2021,
```python
time_start = "2021-01-01T00:00:00Z"
time_end = "2021-02-01T23:59:59Z"
```
Then you could perform a STAC query yourself with those parameters like so:
```python
catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
```
and then include the response in the request parameters under the `catalog` field:
```python
rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["B02"]}}
```
In general, if you are using the SlideRule Python Client to query for the catalog, then there is no difference in just supplying those parameters in your request.  But the example above highlights the ability to take control of the query on the client side.