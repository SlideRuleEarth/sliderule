====================
SlideRule Python API
====================

The SlideRule Python API is used to access the services provided by the base SlideRule server. From Python, the module can be imported via:

.. code-block:: python

    import sliderule

Service Architecture
####################

A typical SlideRule deployment includes three components:

#. Access to the NASA Common Metadata Repository (CMR) system
#. A service discovery server
#. A set of processing nodes

When a client makes a processing request to SlideRule, it is typical that all three components are involved as follows:

#. The client first makes a spatial and temporal query to the **CMR system** to retrieve a set of resources (i.e. H5 files) that correspond to the region and time period of interest.
#. The client then makes a request to the **service discovery server** to retrieve a list of IP addresses of the available SlideRule **processing nodes**
#. Finally, the client creates a thread pool of workers and fans out the processing of each resource over the available SlideRule **processing nodes**

Of course, these steps do not have to be taken in this order, nor is it a problem to just use one component of the system without the others.  
But these components are designed to compliment each other and provide all of the necessary services needed to perform large processing requests.


De-serialization
################ 

There are two types of SlideRule services distinguished by the type of response they provide: (1) **return** services, (2) **stream** services.

Return
    Return services are accessed via the `GET` HTTP method and return a discrete block of ASCII text, typically formatted as JSON.

    These services can easily be accessed via `curl` or other HTTP-based tools, and contain self-describing data.  
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=False)`` call.

Stream
    Stream services are accessed via the `POST` HTTP method and return a serialized stream of binary records containing the results of the processing request.

    These services are more difficult to work with third-party tools since the returned data must be parsed and the data itself is not self-describing.
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=True)`` call.  Inside this call, the SlideRule Python client
    will take care of any additional service calls needed in order to parse the results and return a self-describing Python dictionary (i.e. the elements of 
    the dictionary are named and structured in such a way as to indicate the type and content of the returned data).

    If you want to process streamed results outside of the SlideRule Python client, then a brief description of the format of the data follows.
    For additional guidance, the hidden `__parse <../../../../sliderule/sliderule.py>`_ function inside the *sliderule.py* source code provides the code which performs
    the stream processing for the SlideRule Python client.

    Each response record is formatted as: `<record length><record type><record data>` where,
    
    record length
        32-bit little endian integer providing the total length of the record that follows (type and data)
    record type
        null-terminated ASCII string containing the name of the record type
    record data
        binary contents of data
    
    In order to know how to process the contents of the **record data**, the user must perform an additional query to the SlideRule ``definition`` service, 
    providing the **record type**.  The **definition** service returns a JSON response object that provides a format definition of the record type that can
    be used by the client to decode the binary record data.  The format of the **definition** response object is:
    
    .. code-block:: python
    
        {   
            "@datasize": # minimum size of record
            "field_1":          
            {
                "type": # data type (see sliderule.basictypes for full definition), or record type if a nested structure
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


Functions
#########


source
------

""""""""""""""""

.. py:function:: sliderule.source(api, parm, stream=False, callbacks={'eventrec': __logeventrec})

    Perform API call to SlideRule service

    :param str api: name of the SlideRule endpoint
    :param dict parm: dictionary of request parameters
    :keyword bool stream: whether the request is a **return** service or a **stream** service (see `De-serialization <./SlideRule.html#de-serialization>`_ for more details)
    :keyword dict callbacks: record type callbacks (advanced use)
    :return: response data

    Example: 

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url(["127.0.0.1"])
        >>> rqst = {
        ...     "time": "NOW",
        ...     "input": "NOW",
        ...     "output": "GPS"
        ... }
        >>> rsps = sliderule.source("time", rqst)
        >>> print(rsps)
        {'time': 1300556199523.0, 'format': 'GPS'}


set_url
-------

""""""""""""""""

.. py:function:: sliderule.set_url(urls):

    Configure sliderule package with URL of service 

    :param str urls: IP address or hostname of SlideRule service (note, there is a special case where the url is provided as a list of strings instead of just a string; when a list is provided, the client hardcodes the set of servers that are used to process requests to the exact set provided; this is used for testing and for local installations and can be ignored by most users)

    Example: 

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url("service.my-sliderule-server.org")


update_available_servers
------------------------

""""""""""""""""

.. py:function:: sliderule.update_available_servers():

    Causes the SlideRule Python client to refresh the list of available processing nodes. This is useful when performing large processing requests where there is time for auto-scaling to change the number of nodes running.
    
    This function does nothing if the client has been initialized with a hardcoded list of servers.

    Example: 

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.update_available_servers()


set_max_errors
--------------

""""""""""""""""

.. py:function:: sliderule.set_max_errors(max_errors):

    Configure sliderule package's maximum number of errors per node setting.  When the client makes a request to a processing node, if there is an error, it will retry the request to a different processing node (if available), but will keep the original processing node in the list of available nodes and increment the number of errors associated with it.  But if a processing node accumulates up to the **max_errors** number of errors, then the node is removed from the list of available nodes and will not be used in future processing requests.

    A call to ``update_available_servers`` or ``set_url`` is needed to restore a removed node to the list of available servers.
    
    :param int max_errors: sets the maximum number of errors per node

    Example: 

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_max_errors(3)


gps2utc
-------

""""""""""""""""

.. py:function:: sliderule.gps2utc(gps_time, as_str=True):

    Convert a GPS based time returned from SlideRule into a UTC time.

    :param int gps_time: number of seconds since GPS epoch (January 6, 1980)
    :param bool as_str: if True, returns the time as a string; if False, returns the time as datatime object
    :return: UTC time (i.e. GMT, or Zulu time)

    Example: 

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.gps2utc(1235331234)
        '2019-02-27 19:34:03'





Endpoints
#########


definition
----------

""""""""""""""""

``GET /source/definition <request payload>``

    Gets the record definition of a record type; used to parse binary record data

**Request Payload** *(application/json)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

JSON object defining the on-the-wire binary format of the record data contained in the specified record type.

See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to use the record definitions.



event
-----

""""""""""""""""

``POST /source/event <request payload>``

    Return event messages (logs, traces, and metrics) in real-time that have occurred during the time the request is active

**Request Payload** *(application/json)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - parameter
     - description
     - default
   * - **type**
     - type of event message to monitor: "LOG", "TRACE", "METRIC"
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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Serialized stream of event records of the type ``eventrec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.




geo
---

""""""""""""""""

``GET /source/geo <request payload>``

    Perform geospatial operations on spherical and polar coordinates

**Request Payload** *(application/json)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
     - a box defined by a lower left latitude/longitude pair, and an upper right lattitude/longitude pair
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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

""""""""""""""""

``POST /source/h5 <request payload>``

    Reads a dataset from an HDF5 file and return the values of the dataset in a list.

    See `icesat2.h5 <./ICESat-2.html#h5>`_ function for a convient method for accessing HDF5 datasets.

**Request Payload** *(application/json)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
    >>> sliderule.set_url(["127.0.0.1"])
    >>> asset = "atlas-local"
    >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
    >>> dataset = "/gt1r/geolocation/segment_ph_cnt"
    >>> rqst = {
    ...         "asset" : asset,
    ...         "resource": resource,
    ...         "dataset": dataset,
    ...         "datatype": sliderule.datatypes["INTEGER"],
    ...         "col": 0,
    ...         "startrow": 0,
    ...         "numrows": 5,
    ...         "id": 0
    ...     }
    >>> rsps = sliderule.source("h5", rqst, stream=True)
    >>> print(rsps)
    [{'@rectype': 'h5dataset', 'datatype': 2, 'data': (245, 0, 0, 0, 7, 1, 0, 0, 17, 1, 0, 0, 1, 1, 0, 0, 4, 1, 0, 0), 'size': 20, 'offset': 0, 'id': 0}]

**Response Payload** *(application/octet-stream)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Serialized stream of H5 dataset records of the type ``h5dataset``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.




index
-----

""""""""""""""""

``GET /source/index <request payload>``

    Return list of resources (i.e H5 files) that match the query criteria.

    Since the way resources are indexed (e.g. which meta-data to use), is very dependent upon the actual resources available; this endpoint is not necessarily
    useful in and of itself.  It is expected that data specific indexes will be built per SlideRule deployment, and higher level routines will be constructed
    that take advantage of this endpoint and provide a more meaning interface.

**Request Payload** *(application/json)*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

JSON object containing a list of the resources available to the SlideRule deployment that match the query criteria.

.. code-block:: python

    {
        "resources": ["<resource name>", ...]
    }



time
-----

""""""""""""""""

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
        UTC time represented as a one of two date strings
        
        "<year>:<month>:<day of month>:<hour in day>:<minute in hour>:<second in minute>""
        
        "<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>"

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
