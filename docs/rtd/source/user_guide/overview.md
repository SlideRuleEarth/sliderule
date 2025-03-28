# Overview

## 1. Service Architecture

SlideRule is a **real-time** science data processing service; when a user makes a request to SlideRule, the response is returned right away.  In technical terms, this means that the HTTP response data is sent back over the same TCP/IP connection that was used for the HTTP request.  This is in contrast to a job based processing service where job requests are issued by users, and then at some later time the user is notified that their data is ready and available for download.

While SlideRule supports large processing requests that may take minutes (or even hours), the same paradigm holds.  Client software makes requests to SlideRule, SlideRule immediately begins processing the request and generating response data, and that data is streamed back to the user while the client software maintains an active connection with the SlideRule server.

## 2. Python Client

Using SlideRule means issuing HTTP requests to SlideRule's endpoints to perform processing requests and receive science data results.  But in practice, a typical workflow involves much more than issuing requests.  A typical workflow involves:

1. Defining an *area of interest* as a geographic region over a period of time
2. Determining *which data* is available in that area of interest
3. Describing *what processing* you want to perform
4. Issuing the processing *request*
5. Parsing the *response* data into a usable data structure

The SlideRule Python Client helps make the above steps a lot easier by providing a user-friendly interface for accomplishing typical workflows.

1. The Python client supports an *area of interest* being defined in multiple ways - as a dictionary of latitude and longitudes, a list of coordinates, a GeoJSON file, a Shapefile, or a GeoDataFrame - and then converting that representation into the different forms required by SlideRule and other NASA web services.
2. The Python client supports querying NASA's Common Metadata Repository (CMR) system resources to process given the user's request. Typically, this is performed automatically by the server, but in some cases the user may wish to have greater control over exactly which resources are processed.
3. The Python client allows users to define their processing parameters as Python dictionaries, and make requests to SlideRule using Python functions.
4. The Python client handles the HTTPS connection to the SlideRule servers as well as any necessary authentication requests to the SlideRule Provisioning System when private clusters are being used.
5. The Python client parses the response data from SlideRule and presents a standard GeoDataFrame result back to the user.

## 3. Processing Requests

A SlideRule processing request has four main components, which will be gone over in detail in the sections below:
* endpoint
* parameters
* area of interest
* resources

These four items are encoded into a typical HTTP request to the Sliderule servers.  The SlideRule servers then respond by executing the specified endpoint with the given parameters over the area of interest for each of the supplied resources.  The result is the streamed back to the user as an HTTP response.

## 4. Endpoint

Endpoints in SlideRule are Lua scripts that define a processing function that the user wants to execute.  Technically, each endpoint is the last path item in the url when making an HTTP request to SlideRule.  For example, if a user makes a processing request for the `atl06s` endpoint, that translates into a `POST https://sliderule.slideruleearth.io/source/atl06s` and the execution of the `atl06s.lua` script within the SlideRule server-side system.

There are many endpoints in SlideRule (and more are added as new functionality is developed), most important endpoints have a corresponding function in the Python SlideRule Client that hides which endpoint is being called.  For example, the `icesat2.atl06p` function performs a __POST__ to the `atl06p` endpoint, but that is taken care of automatically by the Python code.  Another example is the `definition` endpoint which provides low-level information about the response structures being returned to the user.  In all cases, the Python SlideRule Client will issue requests to that endpoint behind the scenes when it needs information on how to interpret a response, and the user never needs to know about it.

But there are cases when a user does supply the name of the endpoint in a processing request - the two cases are when the `sliderule.source` and the `sliderule.run` functions are used.  The `sliderule.source` function provides the ability to call any endpoint directly.  The `sliderule.run` function is a high-level interface for making SlideRule requests that work on dataframes and always return a GeoDataFrame; because all of those endpoints are so similar, instead of having a separate Python function for each one, there is one Python function and the first parameter is a string specifying which endpoint to call (but note, unlike the `sliderule.source` function, the `sliderule.run` function only works with some endpoints).

:::{note}
As a fun exercise, if you have access to a bash shell and cURL, you can issue the following command at your shell prompt to get the current version information from the SlideRule public cluster.
```bash
curl https://sliderule.slideruleearth.io/source/version
```
:::


## 5. Parameters

## 6. Area of Interest

## 7. Resources

When accessing SlideRule as a service, there are times when you need to specify which source datasets it should use when processing the data.
A source dataset is called an **asset** and is specified by its name as a string.  SlideRule's asset directory is a list of datasets that SlideRule
has access to.  Each entry in the asset directory describes a dataset and provides the necessary information to find, authenticate, and read that dataset.

The following datasets are currently provided in SlideRule's Asset Directory (with more being added as time goes on);
the ones marked as rasters can be sampled; the ones that are not marked as rasters can be subsetted through different subsetting APIs.

.. csv-table::
    :header: asset, raster, description, location

    icesat2, , The ICESat-2 Standard Data Products ATL03/ATL06/ATL08, nsidc-cumulus-prod-protected
    gedil4a, , GEDI L4A Footprint Level Aboveground Biomass Density, ornl-cumulus-prod-protected
    gedil4b, x, GEDI L4B Gridden Aboveground Biomass Density, ornl-cumulus-prod-protected
    gedil3-elevation, x, GEDI L3 gridded ground elevation, ornl-cumulus-prod-protected
    gedil3-canopy, x, GEDI L3 gridded canopy height, ornl-cumulus-prod-protected
    gedil3-elevation-stddev, x, GEDI L3 gridded ground elevation-standard deviation, ornl-cumulus-prod-protected
    gedil3-canopy-stddev, x, GEDI L3 gridded canopy heigh-standard deviation, ornl-cumulus-prod-protected
    gedil3-counts, x, GEDI L3 gridded counts of valid laser footprints, ornl-cumulus-prod-protected
    gedil2a, , GEDI L2A Elevation and Height Metrics Data Global Footprint, sliderule
    gedil1b, , GEDI L1B Geolocated Waveforms, sliderule
    merit-dem, x, MERIT Digital Elevation Model, sliderule
    swot-sim-ecco-llc4320, x, Simulated SWOT Data, podaac-ops-cumulus-protected
    swot-sim-glorys, x, Simulated SWOT Data, podaac-ops-cumulus-protected
    usgs3dep-1meter-dem, x, USGS 3DEP 1m Digital Elevation Model, prd-tnm
    esa-worldcover-10meter, x, Worldwide land cover mapping, esa-worldcover
    meta-globalcanopy-1meter, x, Meta and World Resources Institute 1m global canopy height map, dataforgood-fb-data
    gebco-bathy, x, General Bathymetric Chart of the Oceans, sliderule
    landsat-hls, x, Harmonized LandSat Sentinal-2, lp-prod-protected
    arcticdem-mosaic, x, PGC Arctic Digital Elevation Model Mosaic, pgc-opendata-dems
    arcticdem-strips, x, PGC Arctic Digital Elevation Model Strips, pgc-opendata-dems
    rema-mosaic, x, PGC Reference Elevation Model of Antarctica Mosaic, pgc-opendata-dems
    rema-strips, x, PGC Reference Elevation Model of Antarctica Strips, pgc-opendata-dems
    atlas-s3, , Internal s3-bucket staged ICESat-2 Standard Data Products: ATL03/ATL06/ATL08, sliderule
    nsidc-s3, , Alias for icesat2 asset, nsidc-cumulus-prod-protected
