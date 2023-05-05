# GeoRaster

2023-05-04

## Background

Raster data consists of 2-dimensional datasets that form a grid of square pixels, often called an image.  A common format for storing raster data is TIFF.  GeoTIFF is an extension to the TIFF format that embeds geospatial information into the TIFF file that ties the raster data to a geospatial reference.  COGs are cloud-optimized GeoTIFFs that are internally optimized for access in the cloud.  For more information see https://www.cogeo.org.

## Overview

SlideRule supports sampling raster data at points of interest and including those sampled values alongside its customized data products.  For instance, when performing an ATL06-SR processing run (`atl06p`), the returned GeoDataFrame has a row for each calculated elevation; that row can also include values from different raster datasets that have been sampled at the geolocation of the calculated elevation.

In order to sample a raster dataset, SlideRule must first ascertain which individual raster files in the dataset intersect the point of interest, then obtain credentials to access the identified files, and then lastly, open up those files and read the necessary pixels to calculate the returned sample value.  Unfortunately, most raster datasets are organized slightly differently and require a small amount of specialized code to perform the first step of determining which raster files need to be sampled.  The second step of obtaining credentials also requires some specialized code, but since most of our datasets are in AWS and authenticated through NASA DAACs, most of the authentication code is generic.  But even still, because of this, each raster dataset supported by SlideRule needs to be registered with SlideRule ahead of time and provided in what we call an Asset Directory.

## Asset Directory

SlideRule's asset directory is a list of datasets that SlideRule has access to.  Each entry in the asset directory describes a dataset and provides the necessary information to find, authenticate, and read that dataset.

The following raster datasets are currently provided in SlideRule's Asset Directory (with more being added as time goes on); the ones marked as rasters can be sampled; the ones that are not marked as rasters can be subsetted through different subsetting APIs.

|asset|raster|description|
|:---:|:---:|:---:|
|icesat2| | The ICESat-2 Standard Data Products: ATL03, ATL06, and ATL08|
|gedil4a| | GEDI L4A Footprint Level Aboveground Biomass Density|
|gedil3-elevation| *| GEDI L3 gridded ground elevation|
|gedil3-canopy| *| GEDI L3 gridded canopy height|
|gedil3-elevation-stddev| *| GEDI L3 gridded ground elevation-standard deviation|
|gedil3-canopy-stddev| *| GEDI L3 gridded canopy heigh-standard deviation|
|gedil3-counts| *| GEDI L3 gridded counts of valid laser footprints|
|gedil2a| | GEDI L2A Elevation and Height Metrics Data Global Footprint|
|gedil1b| | GEDI L1B Geolocated Waveforms|
|landsat-hls| *| Harminized LandSat Sentinal-2|
|arcticdem-mosaic| *| PGC Arctic Digital Elevation Model Mosaic|
|arcticdem-strips| *| PGC Arctic Digital Elevation Model Strips|
|rema-mosaic| *| PGC Reference Elevation Model of Antarctica Mosaic|
|rema-strips| *| PGC Reference Elevation Model of Antarctica Strips|
|atlas-local| | Local file-system staged ICESat-2 Standard Data Products: ATL03, ATL06, and ATL08|
|gedi-local| | Local file-system staged GEDI Stadard Data Products: L1B, L2A, L3, L4A, L4B|
|atlas-s3| | Internal s3-bucket staged ICESat-2 Standard Data Products: ATL03, ATL06, and ATL08|
|nsidc-s3| | Alias for "icesat2" asset|

## Sampling

Sampling rasters is controlled by the `samples` field of a request's parameter dictionary.  The `samples` field is itself a dictionary of dictionaries, each describing a raster dataset that needs to be sampled for the given processing request.  The example below specifies two raster datasets that need to be sampled:
```python
    parms {
        "samples" : {
            "mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True},
            "strips": {"asset": "arcticdem-strips", "algorithm": "CubicSpline"}
        }
    }
```
The `mosaic` and `strips` dictionary keys are provided by the user to identify the data returned back to the user in the GeoDataFrame - they can be anything the user wants as long as it unique.  The `asset` dictionary keys are used to identify which asset in SlideRule's asset directory is to be sampled.  The dictionary keys that follow all control how the raster is to be sampled.

SlideRule supports various algorithms for sampling rasters: NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, and Gauss.  These are the algorithms supplied by GDAL and exposed to users through the SlideRule APIs.  In addition to these algorithms, SlideRule also provides the ability to calculate zonal statistics for a given geolocation: count (number of pixels), minimum, maximum, mean, median, standard devation, and median absolute deviation. Lastly, depending on the dataset, SlideRule provides custom bands that can be derived using other bands in the raster file.  For example, the HLS dataset (`landsat-hls`) provides the following three custom bands: "NDSI", "NDVI", "NDWI".

Prior to sampling a raster, SlideRule provides ways to select and filter which rasters are sampled in a dataset.  These include filtering based on time, and file name pattern matching.

For a full list of parameters available to users when sampling rasters, please see the [sampling parameters](./SlideRule.html#raster-sampling).

## Sample Results

The results returned by SlideRule for sampled raster datasets are sent as ancillary datasets which are then merged by the client software into the primary GeoDataFrame. The column name for each of the values below are prepended with the `key` supplied by the user in the processing request.  For example, if the user supplied `"mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True}` in the processing request, then each column associated with the results of sampling the `arcticdem-mosaic` asset would have `mosiac.` prepended to it.

The standard columns added to a GeoDataFrame for each sampled raster dataset are:

- __"value"__: the sampled value from the raster at the point of interest
- __"time"__: the best time provided by the raster dataset for when the sampled value was measured
- __"file_id"__: a number used to identify the name of the file the sample value came from; this is used in conjunction with the `file_directory` provided in the metadata of a GeoDataFrame
- __"flags"__: any flags (if requested) that acompany the sampled data in the raster it was read from

The zonal statistic columns added to a GeoDataFrame for each sampled raster dataset are:

- __"count"__: number of pixels read to calculate sample value
- __"min"__: minimum pixel value of pixels that constributed to sample value
- __"max"__: maximum pixel value of pixels that constributed to sample value
- __"mean"__: average/mean pixel value of pixels that constributed to sample value
- __"median"__: average/median pixel value of pixels that constributed to sample value
- __"stdev"__: standard deviation of pixel values of pixels that constributed to sample value
- __"mad"__: median absolute deviation of pixel values of pixels that constributed to sample value

## Special Case - HLS

Unlike the other datasets supported by SlideRule, the HLS dataset (identified as `landsat-hls` in the Asset Directory) requires the user to first query NASA's EarthData CMR system, and provide the results of that query in the request to SlideRule.

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
Then you would first have to perform a STAC query with those parameters like so:
```python
catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
```
and then include the response in the request parameters under the `catalog` field:
```python
rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["B02"]}}
```