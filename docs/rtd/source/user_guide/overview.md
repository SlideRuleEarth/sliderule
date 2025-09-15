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

## 3. Requests

A SlideRule processing request has four main components, which we will go over in detail in the sections below:
* endpoint
* parameters
* resources
* area of interest

These four components are encoded into a typical HTTP request to the Sliderule servers.  The SlideRule servers then respond by executing the specified __endpoint__ with the given __parameters__ over the __area of interest__ for each of the supplied __resources__.  The result is the streamed back to the user as an HTTP response.

### Endpoint

Endpoints in SlideRule are Lua scripts that define a processing function that the user wants to execute.  Technically, each endpoint is the last path item in the url when making an HTTP request to SlideRule.  For example, if a user makes a processing request for the `atl06s` endpoint, that translates into a `POST https://sliderule.slideruleearth.io/source/atl06s` and the execution of the `atl06s.lua` script within the SlideRule server-side system.

There are many endpoints in SlideRule (and more are added as new functionality is developed), but most important endpoints have a corresponding function in the Python SlideRule Client that hides which endpoint is being called.  For example, the `icesat2.atl06p` function performs a __POST__ to the __atl06p__ endpoint, which is taken care of automatically by the Python code.  Another example is the `definition` endpoint which provides low-level information about the response structures being returned to the user.  In all cases, the Python SlideRule Client will issue requests to that endpoint behind the scenes when it needs information on how to interpret a response, and the user never needs to know about it.

But there are cases when a user does want to supply the name of the endpoint explicitly in a processing request, and for that there are two Python functions provided: `sliderule.source` and `sliderule.run`.  The `sliderule.source` function provides the ability to call any endpoint directly.  The `sliderule.run` function is a high-level interface for making SlideRule requests that work on dataframes and always return a GeoDataFrame; because all of those endpoints are so similar, instead of having a separate Python function for each one, there is one Python function and the first parameter is a string specifying which endpoint to call (but note, unlike the `sliderule.source` function, the `sliderule.run` function only works with some endpoints).

:::{note}
As a fun exercise, if you have access to a bash shell and cURL, you can execute the `version` endpoint by issuing the following command at your shell prompt;  it will return the current version of the SlideRule software from the public cluster.
```bash
curl https://sliderule.slideruleearth.io/source/version
```
:::

### Parameters

SlideRule is an ___on-demand___ science data processing system, which means that the processing steps performed on the data are executed only when a user makes a request.  Because of this, the user can control how that processing is performed by supplying parameters in their request.  Not all parameters need to be defined when making a request - typically only a few parameters are used for any given request; there are reasonable defaults used for each parameter so that only those parameters you want to customize need to be specified.

In the underlying HTTP request, parameters are json objects supplied in the body of the POST request.  At the Python Client level, parameters are passed to SlideRule Python APIs as a dictionary via the `parm` argument.  In the next section on [basic usage](./basic_usage), we will discuss the base set of parameters available for all requests.  But for now, it is important to understand that given the endpoint being called, different parameters are available that define how that endpoint executes.

For example, a call to the `atl06p` endpoint will have parameters specific to the __atl06__ processing algorithm (e.g. `maxi` defines the maximum iterations the least-sequares fitting algorithm will run), it may also include parameters available for all __ICESat-2__ endpoints (e.g. `res` stands for resolution and specifies the along-track distance of photons to aggregate for each result posting), and finally there may be parameters available to all endpoints (e.g. `resources` defines the list of granules or tiles to be processed).  When making a request to `atl06p`, the user can supply a parameter dictionary with these options configured:

```Python
parm = {
    "fit": {"maxi": 5},
    "res": 20,
    "resources": ["ATL03_20181019065445_03150111_006_02.h5"]
}
```

### Resources

When making a request, there are two ways to specify what data needs to be processed, the first is by manually supplying the names of the resources (i.e. filenames); the second is to supply an area of interest and let the SlideRule servers determine which resources needed to be processed.  In both cases, an ___asset___ for those resources must be defined.  An asset is a collection of resources that share a source location, software driver for reading, and credentials to access.

:::{note}
For example, the ICESat-2 ATL03 data hosted by the NSIDC is a single asset in SlideRule (it is identified simply as `icesat2`).  All of the ATL03 data is stored in the NASA Cumulus S3 bucket, is stored as HDF5 files that use the H5Coro driver, and requires Earthdata Login credentials to read.
:::

Typically, for each endpoint, there is a default _asset_ which makes sense to use and the user does not need to worry about it.  But in some cases, a user may wish to override the default _asset_ and specify exactly where the data should be read from.  When that happens, the `asset` parameter in the parameter structure is used.  To find the full list of assets supported by SlideRule, see the [asset directory](https://github.com/SlideRuleEarth/sliderule/blob/main/targets/slideruleearth/asset_directory.csv) in our GitHub repository.

### Area of Interest

As mentioned above, the second way to specify which resources need to be processed is by supplying an area of interest.  An area of interest is a geographical area specified as a list of latitude and longitude coordinates.  All data within the geographical area is processed and returned to the user.

Processing an area of interest can at times be complicated, and for that reason, there are multiple levels of support provided in SlideRule for handling different scenarios a user might need.  , The following functionality is provided in SlideRule for handling areas of interest (ordered from simplest to most complicated):

Polygon with Resources
:   In the request parameters, the user provides a polygon and a list of resources in their request.  The SlideRule servers use the polygon directly to subset all of the resources specified.

Parameters without Resources
:   In the request parameters, the user provides a polygon and/or other resource query parameters (e.g. name filter), without specifying which resources to process.  The SlideRule servers use the _asset_ (either provided as the default for the endpoint or manually supplied in the request) to determine which metadata repositories contain indexes for the source data.  The server-side code then issues a query to the appropriate metadata repository using the parameters in the request to obtain a list of resources to process.

Python Client `toregion` - Raster
:   Using the SlideRule Python Client `toregion` function, the user can provide a raster image which acts as a mask over the area of interest defining which latitude/longitude cells are "on" and which cells are "off".  The source data is subsetted according to the mask.  This is useful for very complicated areas of interest that represent coastlines or islands where a simple polygon is insufficient.

Python Client `toregion` - GeoJson/Shapfile/GeoDataFrame
:   Using the SlideRule Python Client, the user can define their area of interest using a __geojson__, __shapefile__, or __GeoDataFrame__, and still provide a properly formatted polygon in their request to SlideRule by converting the source definition using the `toregion` function.

Python Client Earthdata
:   Instead of letting the SlideRule server-side code handle querying the appropriate metadata repositories to obtain a list of resources to process, the SlideRule Python Client includes the `earthdata` module which provides functions for directly querying NASA's Common Metadata Repository (CMR) and USGS's The National Map (TNM).  When the intended resources are supported by these metadata repositories, the user can query these repositories directly.

Python Client Earthdata - Clusters
:   For extremely large areas of interest that are also very complicated (e.g. entire U.S. west coast coastline), the SlideRule Python Client provides functions in the `earthdata` module to break up the area of interest into smaller areas acceptable to CMR, issue individual CMR requests, and then combine all of the responses back into one list of resources to process.

## 4. Responses

A response from a SlideRule processing request, is an HTTP response provided directly back to the user with the data results in the payload of the response packets. There are three types of SlideRule responses corresponding to the three types of endpoints SlideRule supports: (1) normal endpoints, (2) arrow endpoints, (3) streaming endpoints.

#### Normal

Normal endpoints are accessed via the GET HTTP method on the `/source` path and return a discrete block of ASCII text, typically formatted as JSON.

These services can be easily accessed via curl or other HTTP-based tools, and contain self-describing data. When using the SlideRule Python client, they are accessed via the `sliderule.source(..., stream=False)` call.

#### Arrow

Arrow endpoints are accessed via the POST HTTP method on the `/arrow` path and return an Apache Arrow supported format: parquet, csv, or feather.

These services can be easily accessed via curl or other HTTP-based tools, but are not directly supported using the SlideRule Python client.  The reason is that they are designed specifically so that a native client is not needed; both `cURL` and the Python `requests` package are sufficient for working this these services.  You can find both [cURL examples](https://github.com/SlideRuleEarth/sliderule/tree/main/clients/curl/examples) and [Python examples](https://github.com/SlideRuleEarth/sliderule/tree/main/clients/python/examples) in the SlideRule git repository.

Any SlideRule endpoint accessible via the `/arrow` path is also accessible via the `/source` via the streaming method described below.  The streaming method also allows for the data to be returned in an Apache Arrow supported format; so no functionality is lost.

#### Streaming

Streaming endpoints are accessed via the POST HTTP method on the `source` path and return a serialized stream of binary records containing the results of the processing request.

These responses contain much richer information (log messages, errors, support for multiple files), but are more difficult to work with third-party tools since the returned data must be parsed and the data itself is not self-describing. When using the SlideRule Python client, these services are accessed via the `sliderule.source(..., stream=True)` call, along with the many other API functions which make it easier to work with these services. The SlideRule Python client takes care of any additional service calls needed in order to parse the streaming results and return a self-describing Python data structure (i.e. the elements of the data structure are named in such a way as to indicate the type and content of the returned data).

:::{note}
If you want to process streamed results outside of the SlideRule Python Client, then a brief description of the format of the data follows. For additional guidance, the hidden functions inside the sliderule.py source code provide the code which performs the stream processing for the SlideRule Python Client.

Each response record is formatted as: <record header><record type><record data> where,

record header
:   64-bit big endian structure providing the version and length of the record: <version:16 bits><type size:16 bits><data size:32 bits>.

record type
:   null-terminated ASCII string containing the name of the record type

record data
:   binary contents of data

In order to know how to process the contents of the record data, the user must perform an additional query to the SlideRule definition service, providing the record type. The definition service returns a JSON response object that provides a format definition of the record type that can be used by the client to decode the binary record data. The format of the definition response object is:

```Python
{
    "__datasize": # minimum size of record
    "field_1":
    {
        "type": # data type (see session.BASIC_TYPES for full definition), or record type if a nested structure
        "elements": # number of elements, 1 if not an array
        "offset": # starting bit offset into record data
        "flags": # processing flags - LE: little endian, BE: big endian, PTR: pointer
    },
    ...
    "field_n":
    {
        ...
    }
}
```
:::
