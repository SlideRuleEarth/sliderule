Endpoints
############

definition
----------

``GET /source/definition <request payload>``

    Gets the record definition of a record type; used to parse binary record data

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **record-type**
         - the name of the record type
         - *required*

    **HTTP Example**

    .. code-block:: http

        GET /source/definition HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 23


        {"rectype": "atl03rec"}

    **Python Example**

    .. code-block:: python

        # Request Record Definition
        rsps = sliderule.source("definition", {"rectype": "atl03rec"}, stream=False)

**Response Payload** *(application/json)*

    JSON object defining the on-the-wire binary format of the record data contained in the specified record type.

    See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to use the record definitions.



event
-----

``POST /source/event <request payload>``

    Return event messages (logs, traces, and metrics) in real-time that have occurred during the time the request is active

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **type**
         - type of event message to monitor: "LOG", "TRACE", "TELEMETRY"
         - "LOG"
       * - **level**
         - minimum event level to monitor: "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
         - "INFO"
       * - **format**
         - the format of the event message: "FMT_TEXT", "FMT_JSON"; empty for binary record representation
         - *optional*
       * - **duration**
         - seconds to hold connection open
         - 0

    **HTTP Example**

    .. code-block:: http

        POST /source/event HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 48

        {"type": "LOG", "level": "INFO", "duration": 30}

    **Python Example**

    .. code-block:: python

        # Build Logging Request
        rqst = {
            "type": "LOG",
            "level" : "INFO",
            "duration": 30
        }

        # Retrieve logs
        rsps = sliderule.source("event", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of event records of the type ``logrec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



geo
---

``GET /source/geo <request payload>``

    Perform geospatial operations on spherical and polar coordinates

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source (see `Assets <#assets>`_)
         - *required*
       * - **pole**
         - polar orientation of indexing operations: "north", "south"
         - "north"
       * - **lat**
         - spherical latitude coordinate to project onto a polar coordinate system, -90.0 to 90.0
         - *optional*
       * - **lon**
         - spherical longitude coordinate to project onto a polar coordinate system, -180.0 to 180.0
         - *optional*
       * - **x**
         - polar x coordinate to project onto a spherical coordinate system
         - *optional*
       * - **y**
         - polar y coordinate to project onto a spherical coordinate system
         - *optional*
       * - **span**
         - a box defined by a lower left latitude/longitude pair, and an upper right latitude/longitude pair
         - *optional*
       * - **span1**
         - a span used for intersection with the span2
         - *optional*
       * - **span2**
         - a span used for intersection with the span1
         - *optional*

    .. list-table:: span definition
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **lat0**
         - smallest latitude (starting at -90.0)
         - *required*
       * - **lon0**
         - smallest longitude (starting at -180.0)
         - *required*
       * - **lat1**
         - largest latitude (ending at 90.0)
         - *required*
       * - **lon1**
         - largest longitude (ending at 180.0)
         - *required*

    **HTTP Example**

    .. code-block:: http

        GET /source/geo HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 115


        {"asset": "atlas-local", "pole": "north", "lat": 30.0, "lon": 100.0, "x": -0.20051164424058, "y": -1.1371580426033}

    **Python Example**

    .. code-block:: python

        rqst = {
            "asset": "atlas-local",
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": -1.1371580426033,
        }

        rsps = sliderule.source("geo", rqst)


**Response Payload** *(application/json)*

    JSON object with elements populated by the inferred operations being requested

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **intersect**
         - true if span1 and span2 intersect, false otherwise
         - *optional*
       * - **combine**
         - the combined span of span1 and span 2
         - *optional*
       * - **split**
         - the split of span
         - *optional*
       * - **lat**
         - spherical latitude coordinate projected from the polar coordinate system, -90.0 to 90.0
         - *optional*
       * - **lon**
         - spherical longitude coordinate projected from the polar coordinate system, -180.0 to 180.0
         - *optional*
       * - **x**
         - polar x coordinate projected from the spherical coordinate system
         - *optional*
       * - **y**
         - polar y coordinate projected from the spherical coordinate system
         - *optional*

    **HTTP Example**

    .. code-block:: http

        HTTP/1.1 200 OK
        Server: sliderule/0.5.0
        Content-Type: text/plain
        Content-Length: 76


        {"y":1.1371580426033,"x":-0.20051164424058,"lat":29.999999999998,"lon":-100}



h5
--

``POST /source/h5 <request payload>``

    Reads a dataset from an HDF5 file and return the values of the dataset in a list.

    See `h5.h5 </web/rtd/api_reference/h5.html#h5>`_ function for a convient method for accessing HDF5 datasets.

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source asset (see `Assets <#assets>`_)
         - *required*
       * - **resource**
         - HDF5 filename
         - *required*
       * - **dataset**
         - full path to dataset variable
         - *required*
       * - **datatype**
         - the type of data the returned dataset values should be in
         - "DYNAMIC"
       * - **col**
         - the column to read from the dataset for a multi-dimensional dataset
         - 0
       * - **startrow**
         - the first row to start reading from in a multi-dimensional dataset
         - 0
       * - **numrows**
         - the number of rows to read when reading from a multi-dimensional dataset
         - -1 (all rows)
       * - **id**
         - value to echo back in the records being returned
         - 0

    **HTTP Example**

    .. code-block:: http

        POST /source/h5 HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 189


        {"asset": "atlas-local", "resource": "ATL03_20181019065445_03150111_003_01.h5", "dataset": "/gt1r/geolocation/segment_ph_cnt", "datatype": 2, "col": 0, "startrow": 0, "numrows": 5, "id": 0}


    **Python Example**

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> asset = "icesat2"
        >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
        >>> dataset = "/gt1r/geolocation/segment_ph_cnt"
        >>> rqst = {
        "asset" : asset,
        "resource": resource,
        "dataset": dataset,
        "datatype": sliderule.datatypes["INTEGER"],
        "col": 0,
        "startrow": 0,
        "numrows": 5,
        "id": 0
        }
        >>> rsps = sliderule.source("h5", rqst, stream=True)
        >>> print(rsps)
        [{'__rectype': 'h5dataset', 'datatype': 2, 'data': (245, 0, 0, 0, 7, 1, 0, 0, 17, 1, 0, 0, 1, 1, 0, 0, 4, 1, 0, 0), 'size': 20, 'offset': 0, 'id': 0}]

**Response Payload** *(application/octet-stream)*

    Serialized stream of H5 dataset records of the type ``h5dataset``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.




h5p
---

``POST /source/h5p <request payload>``

    Reads a list of datasets from an HDF5 file and returns the values of the datasets in a dictionary of lists.

    See `h5.h5p </web/rtd/api_reference/h5.html#h5p>`_ function for a convient method for accessing HDF5 datasets.

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source asset (see `Assets <#assets>`_)
         - *required*
       * - **resource**
         - HDF5 filename
         - *required*
       * - **datasets**
         - list of datasets (see `h5 <#h5>`_ for a list of parameters for each dataset)
         - *required*

    **Python Example**

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> asset = "icesat2"
        >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
        >>> dataset = "/gt1r/geolocation/segment_ph_cnt"
        >>> datasets = [ {"dataset": dataset, "col": 0, "startrow": 0, "numrows": 5} ]
        >>> rqst = {
        "asset" : asset,
        "resource": resource,
        "datasets": datasets,
        }
        >>> rsps = sliderule.source("h5p", rqst, stream=True)
        >>> print(rsps)
        [{'__rectype': 'h5file', 'dataset': '/gt1r/geolocation/segment_ph_cnt', 'elements': 5, 'size': 20, 'datatype': 2, 'data': (245, 0, 0, 0, 7, 1, 0, 0, 17, 1, 0, 0, 1, 1, 0, 0, 4, 1, 0, 0)}]

**Response Payload** *(application/octet-stream)*

    Serialized stream of H5 file data records of the type ``h5file``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



health
------

``GET /source/health``

    Provides status on the health of the node.

**Response Payload** *(application/json)*

    JSON object containing a true|false indicator of the health of the node.

    .. code-block:: python

        {
            "healthy": true|false
        }



index
-----

``GET /source/index <request payload>``

    Return list of resources (i.e H5 files) that match the query criteria.

    Since the way resources are indexed (e.g. which meta-data to use), is very dependent upon the actual resources available; this endpoint is not necessarily
    useful in and of itself.  It is expected that data specific indexes will be built per SlideRule deployment, and higher level routines will be constructed
    that take advantage of this endpoint and provide a more meaningful interface.

**Request Payload** *(application/json)*

    .. code-block:: python

            {
                "or"|"and":
                {
                    "<index name>": { <index parameters>... }
                    ...
                }
            }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **index name**
         - name of server-side index to use (deployment specific)
         - *required*
       * - **index parameters**
         - an index span represented in the format native to the index selected
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a list of the resources available to the SlideRule deployment that match the query criteria.

    .. code-block:: python

        {
            "resources": ["<resource name>", ...]
        }



metric
------

``GET /source/metric <request payload>``

    Return a list of metric values associated with a provided system attribute.

    Each SlideRule server node maintains internal metrics on a variety of things.  Each metric is associated with an attribute that identifies a set of metrics.

    When querying metrics, you provide the metric attribute, and the server will respond with the set of metrics associated with that attribute.

**Request Payload** *(application/json)*

    .. code-block:: python

      {
        "attr": <metric attribute>
      }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **metric attribute**
         - name of the attribute that is being queried
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a set of the metric names and values.

    .. code-block:: python

        {
            "<metric name>": <metric value>,
            ...
        }



tail
----

``GET /source/tail <request payload>``

    Retrieve the most recent log messages generated by the server.

    The number of log message saved by the server is configured at startup.  This endpoint will return up to the maximum number of log messages that are saved.

**Request Payload** *(application/json)*

    .. code-block:: python

      {
        "monitor": "<monitor name>"
      }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **monitor**
         - name of the monitor to tail, should almost always be "EventMonitor"
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a list of log messages.

    .. code-block:: python

        [
            "<log message 1>",
            "<log message 2>",
            ...
            "<log message N>"
        ]



time
-----

``GET /source/time <request payload>``

    Converts times from one format to another

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **time**
         - time value
         - *required*
       * - **input**
         - format of above time value: "NOW", "CDS", "GMT", "GPS"
         - *required*
       * - **output**
         - desired format of return value: same as above
         - *required*

    Sliderule supports the following time specifications

    NOW
        If supplied for either input or time then grab the current time

    CDS
        CCSDS 6-byte packet timestamp represented as [<day>, <ms>]

        days = 2 bytes of days since GPS epoch

        ms = 4 bytes of milliseconds in the current day

    GMT
        UTC time represented as "<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>"

    DATE
        UTC time represented as "<year>-<month>-<day of month>T<hour in day>:<minute in hour>:<second in minute>Z""

    GPS
        seconds since GPS epoch "January 6, 1980"


    **HTTP Example**

    .. code-block:: http

        GET /source/time HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 48


        {"time": "NOW", "input": "NOW", "output": "GPS"}

    **Python Example**

    .. code-block:: python

        rqst = {
            "time": "NOW",
            "input": "NOW",
            "output": "GPS"
        }

        rsps = sliderule.source("time", rqst)

**Response Payload** *(application/json)*

    JSON object describing the results of the time conversion

    .. code-block:: python

        {
            "time":     <time value>
            "format":   "<format of time value>"
        }


version
-------

``GET /source/version``

    Get the version information of the server.

**Response Payload** *(application/json)*

    JSON object containing the version information.

    .. code-block:: python

      {
          "server": {
              "packages": [
                  "<package 1>",
                  "<package 2>",
                  ...
                  "<package n>"
              ],
              "version": "<version string>",
              "launch": "<date of launch>",
              "commit": "<commit id of code>",
              "duration": <seconds since launch>
          }
          "<package 1>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          },
          "<package 2>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          },
          ...
          "<package n>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          }
      }
