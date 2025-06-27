# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import logging
import warnings
import numpy
import tempfile
import json
import geopandas
from shapely.geometry import Polygon
from shapely.geometry.multipolygon import MultiPolygon
from datetime import datetime
from sliderule import version
from sliderule.session import Session, BASIC_TYPES, CODED_TYPE, FatalError, RetryRequest

try:
    from sklearn.cluster import KMeans
except:
    # clustering unsupported in `toregion` function
    # n_clusters restricted to a value of 1
    pass

###############################################################################
# GLOBALS
###############################################################################

DEFAULT_CRS = "EPSG:4326"
logger = logging.getLogger(__name__)
slideruleSession = Session()

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (
    url=Session.PUBLIC_URL,
    verbose=False,
    loglevel=logging.INFO,
    organization=0,
    desired_nodes=None,
    time_to_live=60,
    bypass_dns=False,
    plugins=[],
    log_handler=None,
    rethrow=False ):
    '''
    Initializes the Python client for use with SlideRule, and should be called before other ICESat-2 API calls.
    This function is a wrapper for a handful of sliderule functions that would otherwise all have to be called in order to initialize the client.

    Parameters
    ----------
        url:            str
                        the IP address or hostname of the SlideRule service (slidereearth.io by default)
        verbose:        bool
                        sets up console logger as a convenience to user so all logs are printed to screen
        loglevel:       int
                        minimum severity of log message to output
        organization:   str
                        SlideRule provisioning system organization the user belongs to (see sliderule.authenticate for details)
        desired_nodes:  int
                        requested number of processing nodes in the cluster
        time_to_live:   int
                        minimum number of minutes the desired number of nodes should be present in the cluster
        bypass_dns:     bool
                        if true then the ip address for the cluster is retrieved from the provisioning system and used directly
        plugins:        list
                        names of the plugins that need to be available on the server
        log_handler:    logger
                        user provided logging handler
        rethrow:        bool
                        client rethrows exceptions to be handled by calling code

    Returns
    -------
    bool
        Status of version check

    Examples
    --------
        >>> import sliderule
        >>> sliderule.init()
    '''
    global slideruleSession
    # create new global session
    slideruleSession = Session(
        domain=url,
        verbose=verbose,
        loglevel=loglevel,
        organization=organization,
        desired_nodes=desired_nodes,
        time_to_live=time_to_live,
        bypass_dns=bypass_dns,
        log_handler=log_handler,
        rethrow=rethrow)
    # verify compatibility between client and server versions
    return check_version(plugins=plugins)

#
#  source
#
def source (api, parm={}, stream=False, callbacks={}, path="/source", session=None):
    '''
    Perform API call to SlideRule service

    Parameters
    ----------
        api:        str
                    name of the SlideRule endpoint
        parm:       dict
                    dictionary of request parameters
        stream:     bool
                    whether the request is a **normal** service or a **stream** service (see `De-serialization </web/rtd/user_guide/SlideRule.html#de-serialization>`_ for more details)
        callbacks:  dict
                    record type callbacks (advanced use)
        path:       str
                    path to api being requested

    Returns
    -------
    dictionary
        response data

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> rqst = {
        ...     "time": "NOW",
        ...     "input": "NOW",
        ...     "output": "GPS"
        ... }
        >>> rsps = sliderule.source("time", rqst)
        >>> print(rsps)
        {'time': 1300556199523.0, 'format': 'GPS'}
    '''
    session = checksession(session)
    for tolerance in [0.0001, 0.001, 0.01, 0.1, 1.0, None]:
        try:
            return session.source(api, parm=parm, stream=stream, callbacks=callbacks, path=path)
        except RetryRequest as e:
            logger.info(f'Retry requested by {api}: {e}')
            simplifypolygon(parm, tolerance)
    return None

#
#  set_url
#
def set_url (domain, session=None):
    '''
    Configure sliderule package with URL of service

    Parameters
    ----------
        urls:   str
                IP address or hostname of SlideRule service (note, there is a special case where the url is provided as a list of strings
                instead of just a string; when a list is provided, the client hardcodes the set of servers that are used to process requests
                to the exact set provided; this is used for testing and for local installations and can be ignored by most users)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("service.my-sliderule-server.org")
    '''
    session = checksession(session)
    session.service_domain = domain

#
#  set_verbose
#
def set_verbose (enable, loglevel=logging.INFO, session=None):
    '''
    Sets up a console logger to print log messages to screen

    If you want more control over the behavior of the log messages being captured, do not call this function but instead
    create and configure a Python log handler of your own and attach it to `sliderule.logger`.

    Parameters
    ----------
        enable:     bool
                    True: creates console logger if it doesn't exist, False: destroys console logger if it does exist

        loglevel:       int
                        minimum severity of log message to output

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_verbose(True, loglevel=logging.INFO)
    '''
    session = checksession(session)
    session.set_verbose(enable, loglevel)

#
# set_rqst_timeout
#
def set_rqst_timeout (timeout, session=None):
    '''
    Sets the TCP/IP connection and reading timeouts for future requests made to sliderule servers.
    Setting it lower means the client will failover more quickly, but may generate false positives if a processing request stalls or takes a long time returning data.
    Setting it higher means the client will wait longer before designating it a failed request which in the presence of a persistent failure means it will take longer for the client to remove the node from its available servers list.

    Parameters
    ----------
        timeout:    tuple
                    (<connection timeout in seconds>, <read timeout in seconds>)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_rqst_timeout((10, 60))
    '''
    session = checksession(session)
    if type(timeout) == tuple:
        session.rqst_timeout = timeout
    else:
        raise FatalError('timeout must be a tuple (<connection timeout>, <read timeout>)')

#
# set_processing_flags
#
def set_processing_flags (aux=True, session=None):
    '''
    Sets flags used when processing the record definitions

    Parameters
    ----------
        aux:    bool
                decode auxiliary fields

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_processing_flags(aux=False)
    '''
    session = checksession(session)
    if type(aux) == bool:
        session.decode_aux = aux
    else:
        raise FatalError('aux must be a boolean')


#
# update_available_servers
#
def update_available_servers (desired_nodes=None, time_to_live=None, session=None):
    '''
    Manages the number of servers in the cluster.
    If the desired_nodes parameter is set, then a request is made to change the number of servers in the cluster to the number specified.
    In all cases, the number of nodes currently running in the cluster are returned - even if desired_nodes is set;
    subsequent calls to this function is needed to check when the current number of nodes matches the desired_nodes.

    Parameters
    ----------
        desired_nodes:  int
                        the desired number of nodes in the cluster
        time_to_live:   int
                        number of minutes for the desired nodes to run

    Returns
    -------
    int
        number of nodes currently in the cluster
    int
        number of nodes available for work in the cluster

    Examples
    --------
        >>> import sliderule
        >>> num_servers, max_workers = sliderule.update_available_servers(10)
    '''
    session = checksession(session)
    return session.update_available_servers(desired_nodes, time_to_live)

#
# scaleout
#
def scaleout(desired_nodes, time_to_live, bypass_dns, session=None):
    '''
    Scale the cluster and wait for cluster to reach desired state

    Parameters
    ----------
        desired_nodes:  int
                        the desired number of nodes in the cluster
        time_to_live:   int
                        number of minutes for the desired nodes to run
        bypass_dns:     bool
                        the cluster ip address is retrieved from the provisioning system and used directly

    Examples
    --------
        >>> import sliderule
        >>> sliderule.scaleout(4, 300, False)
    '''
    session = checksession(session)
    return session.scaleout(desired_nodes, time_to_live, bypass_dns)

#
# authenticate
#
def authenticate (ps_organization, ps_username=None, ps_password=None, github_token=None, session=None):
    '''
    Authenticate to SlideRule Provisioning System
    The username and password can be provided the following way in order of priority:
    (1) The passed in arguments `github_token` or `ps_username` and `ps_password`;
    (2) The O.S. environment variables `PS_GITHUB_TOKEN` or `PS_USERNAME` and `PS_PASSWORD`;
    (3) The `ps.<url>` entry in the .netrc file in your home directory

    Parameters
    ----------
        ps_organization:    str
                            name of the SlideRule organization the user belongs to

        ps_username:        str
                            SlideRule provisioning system account name

        ps_password:        str
                            SlideRule provisioning system account password

        github_token:       str
                            GitHub access token (minimum scope/permissions require)

    Returns
    -------
    status
        True of successful, False if unsuccessful

    Examples
    --------
        >>> import sliderule
        >>> sliderule.authenticate("myorg")
        True
    '''
    session = checksession(session)
    return session.authenticate(ps_organization, ps_username, ps_password, github_token)

#
# gps2utc
#
def gps2utc (gps_time, as_str=True, session=None):
    '''
    Convert a GPS based time returned from SlideRule into a UTC time.

    Parameters
    ----------
        gps_time:   float
                    number of seconds since GPS epoch (January 6, 1980)
        as_str:     bool
                    if True, returns the time as a string; if False, returns the time as datatime object

    Returns
    -------
    datetime
        UTC time (i.e. GMT, or Zulu time)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.gps2utc(1235331234)
        '2019-02-27 19:34:03'
    '''
    session = checksession(session)
    rsps = session.source("time", {"time": int(gps_time * 1000), "input": "GPS", "output": "DATE"})
    if as_str:
        return rsps["time"]
    else:
        return datetime.strptime(rsps["time"], '%Y-%m-%dT%H:%M:%SZ')

#
# get_version
#
def get_version (session=None):
    '''
    Get the version information for the running servers and Python client

    Returns
    -------
    dict
        dictionary of version information
    '''
    session = checksession(session)
    rsps = session.source("version", {})
    rsps["client"] = {"version": version.full_version, "organization": session.service_org}
    return rsps

#
# check_version
#
def check_version (plugins=[], session=None):
    '''
    Check that the version of the client matches the version of the server and any additionally requested plugins

    Parameters
    ----------
    plugins:    list
                list of package names (as strings) to check the version on

    Returns
    -------
    bool
        True (always; kept for backward compatibility)
    '''
    session = checksession(session)
    info = get_version(session=session)

    # check response from server
    if info == None:
        raise FatalError(f'Failed to get response from server at {session.service_org}.{session.service_domain}')

    # populate version info
    versions = {}
    for entity in ['server', 'client']:
        s = info[entity]['version'][1:].split('.')
        versions[entity] = (int(s[0]), int(s[1]), int(s[2]))

    # check major version mismatches
    if versions['server'][0] != versions['client'][0]:
        raise FatalError(f'Client {info["client"]["version"]} is incompatible with the server {info["server"]["version"]}')

    # check plugins
    for plugin in plugins:
        if versions[plugin][0] != versions['client'][0]:
            raise FatalError(f'Client {info["client"]["version"]} is incompatible with the {plugin} plugin {info[plugin]["version"]}')

    # check minor version mismatches
    if versions['server'][1] > versions['client'][1]:
        logger.warning(f'Warning, this environment is using an outdated client ({info["client"]["version"]}). The code will run but some functionality supported by the server ({info["server"]["version"]}) may not be available.')

    # return boolean for backward compatibility
    return True

#
# Run: API Request to SlideRule
#
def run(api, parms, aoi=None, resources=None, session=None):
    '''
    Execute the requested endpoint and return the results as a GeoDataFrame

    Parameters
    ----------
        api:        str
                    endpoint to run
        parms:      dict
                    parameter dictionary
        aoi:        dict
                    area of interest, passed to `sliderule.toregion()` function and polygon supplied in request
        resources:  list
                    list of resource names as strings

    Returns
    -------
    GeoDataFrame
        result of executing the request endpoint
    '''
    session = checksession(session)

    # add region
    if aoi != None:
        region = toregion(aoi)
        parms["poly"] = region["poly"]

    # add resources
    if type(resources) == list:
        parms["resources"] = resources

    # manage output for convenience
    delete_tempfile = False
    if "output" not in parms:
        delete_tempfile = True
        parms["output"] = {
            "path": tempfile.mktemp(),
            "format": "geoparquet",
            "open_on_complete": True
        }
    elif ("path" not in parms["output"]) and ("asset" not in parms["output"]):
        delete_tempfile = True
        parms["output"]["path"] = tempfile.mktemp()
        parms["output"]["open_on_complete"] = True
        if "format" not in parms["output"]:
            parms["output"]["format"] = "geoparquet"

    # make request
    rsps = source(api, {"parms": parms}, stream=True, session=session)

    # build geodataframe
    gdf = procoutputfile(parms, rsps)

    # delete tempfile
    if delete_tempfile and os.path.exists(parms["output"]["path"]):
        os.remove(parms["output"]["path"])

    # return geodataframe
    return gdf

#
# Format Region Specification
#
def toregion(source, tolerance=0.0, cellsize=0.01, n_clusters=1):
    '''
    Convert a GeoJSON/Shapefile/GeoDataFrame/list representation of a set of geospatial regions into a list of lat,lon coordinates and raster image recognized by SlideRule

    Parameters
    ----------
        source:     str
                    file name of GeoJSON formatted regions of interest, file **must** have name with the .geojson suffix
                    file name of ESRI Shapefile formatted regions of interest, file **must** have name with the .shp suffix
                    GeoDataFrame of region of interest
                    list of longitude,latitude pairs forming a polygon (e.g. [lat1, lon1, lat2, lon2, lat3, lon3, lat1, lon1])
                    list of longitude,latitude pairs forming a bounding box (e.g. [lat1, lon1, lat2, lon2])
        tolerance:  float
                    tolerance used to simplify complex shapes so that the number of points is less than the limit (a tolerance of 0.001 typically works for most complex shapes)
        cellsize:   float
                    size of pixel in degrees used to create the raster image of the polygon
        n_clusters: int
                    number of clusters of polygons to create when breaking up the request to CMR

    Returns
    -------
    dict
        a list of longitudes and latitudes containing the region of interest that can be used for the **poly** and **raster** parameters in a processing request to SlideRule.

        region = {

            "gdf": <GeoDataFrame of region>

            "poly": [{"lat": <lat1>, "lon": <lon1> }, ...],

            "raster": {"data": <geojson file as string>,

            "clusters": [[{"lat": <lat1>, "lon": <lon1>}, ...], [{"lat": <lat1>, "lon": <lon1>}, ...]] }

    Examples
    --------
    >>> import sliderule, json
    >>> region = sliderule.toregion("tests/data/grandmesa.geojson")
    >>> print(json.dumps(region["poly"], indent=4))
    [
        {
            "lon": -108.20772968780051,
            "lat": 38.8232055291981
        },
        {
            "lon": -108.07460164311031,
            "lat": 38.8475137825863
        },
        {
            "lon": -107.72839858755752,
            "lat": 39.01510930230633
        },
        {
            "lon": -107.78724142490994,
            "lat": 39.195630349659986
        },
        {
            "lon": -108.17287000970857,
            "lat": 39.15920066396116
        },
        {
            "lon": -108.31168256553767,
            "lat": 39.13757646212944
        },
        {
            "lon": -108.34115668325224,
            "lat": 39.03758987613325
        },
        {
            "lon": -108.2878686387796,
            "lat": 38.89051431295789
        },
        {
            "lon": -108.20772968780051,
            "lat": 38.8232055291981
        }
    ]
    '''
    # GeoDataFrame
    if isinstance(source, geopandas.GeoDataFrame):
        gdf = source
        datafile = gdf.to_json()

    # Polygon
    elif isinstance(source, Polygon):
        gdf = geopandas.GeoDataFrame(geometry=[source], crs=DEFAULT_CRS)
        datafile = gdf.to_json()

    # bounding box as [lon lat lon lat]
    elif isinstance(source, list) and (len(source) >= 4) and (len(source) % 2 == 0):
        if len(source) == 4: # bounding box
            lons = [source[0], source[2], source[2], source[0], source[0]]
            lats = [source[1], source[1], source[3], source[3], source[1]]
        elif len(source) > 4: # polygon list
            lons = [source[i] for i in range(1,len(source),2)]
            lats = [source[i] for i in range(0,len(source),2)]
        p = Polygon([point for point in zip(lons, lats)])
        gdf = geopandas.GeoDataFrame(geometry=[p], crs=DEFAULT_CRS)
        datafile = gdf.to_json()

    # List of lat/lon dictionaries
    elif isinstance(source, list) and (len(source) >= 4) and isinstance(source[0], dict):
        p = Polygon([(c["lon"], c["lat"]) for c in source])
        gdf = geopandas.GeoDataFrame(geometry=[p], crs=DEFAULT_CRS)
        datafile = gdf.to_json()

    # Shapefile
    elif isinstance(source, str) and (source.find(".shp") > 1):
        gdf = geopandas.read_file(source)
        datafile = gdf.to_json()

    # GeoJSON file
    elif isinstance(source, str) and (source.find(".geojson") > 1):
        gdf = geopandas.read_file(source)
        with open(source, mode='rt') as file:
            datafile = file.read()

    # GeoJSON dictionary
    elif isinstance(source, dict) and ("features" in source):
        gdf = geopandas.GeoDataFrame.from_features(source["features"])
        datafile = gdf.to_json()

    # GeoJSON string
    elif isinstance(source, str) and (source.find("FeatureCollection") > 0):
        geojson_dict = json.loads(source)
        gdf = geopandas.GeoDataFrame.from_features(geojson_dict["features"])
        datafile = gdf.to_json()

    # Unsupported
    else:
        raise FatalError("incorrect filetype: please use a .geojson, .shp, or a geodataframe")

    # If user provided raster we don't have gdf, geopandas cannot easily convert it
    polygon = clusters = None
    if gdf is not None:
        # simplify polygon
        if(tolerance > 0.0):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                gdf = gdf.buffer(tolerance)
                gdf = gdf.simplify(tolerance)

        # generate polygon
        polygon = gdf2poly(gdf)

        # generate clusters
        clusters = []
        if n_clusters > 1:
            # pull out centroids of each geometry object
            if "CenLon" in gdf and "CenLat" in gdf:
                X = numpy.column_stack((gdf["CenLon"], gdf["CenLat"]))
            else:
                s = gdf.centroid
                X = numpy.column_stack((s.x, s.y))
            # run k means clustering algorithm against polygons in gdf
            kmeans = KMeans(n_clusters=n_clusters, init='k-means++', random_state=5, max_iter=400)
            y_kmeans = kmeans.fit_predict(X)
            k = geopandas.pd.DataFrame(y_kmeans, columns=['cluster'])
            gdf = gdf.join(k)
            # build polygon for each cluster
            for n in range(n_clusters):
                c_gdf = gdf[gdf["cluster"] == n]
                c_poly = gdf2poly(c_gdf)
                clusters.append(c_poly)

    # return region
    return {
        "gdf": gdf,
        "poly": polygon, # convex hull of polygons
        "clusters": clusters, # list of polygon clusters for cmr request
        "raster": {
            "geojson": datafile, # geojson file
            "length": len(datafile), # geojson file length
            "cellsize": cellsize  # units are in crs/projection
        }
    }

###############################################################################
# INTERNAL APIs
###############################################################################

#
# Get Record Field Definition
#
def getdefinition (rectype, fieldname, session=None):
    session = checksession(session)
    return session.get_definition(rectype, fieldname)
#
# GeoDataFrame to Polygon
#
def gdf2poly(gdf):

    # pull out coordinates
    hull = gdf.unary_union.convex_hull
    polygon = [{"lon": coord[0], "lat": coord[1]} for coord in list(hull.exterior.coords)]

    # determine winding of polygon #
    #              (x2               -    x1)             *    (y2               +    y1)
    wind = sum([(polygon[i+1]["lon"] - polygon[i]["lon"]) * (polygon[i+1]["lat"] + polygon[i]["lat"]) for i in range(len(polygon) - 1)])
    if wind > 0:
        # reverse direction (make counter-clockwise) #
        ccw_poly = []
        for i in range(len(polygon), 0, -1):
            ccw_poly.append(polygon[i - 1])
        # replace region with counter-clockwise version #
        polygon = ccw_poly

    # return polygon
    return polygon

#
# Create Empty GeoDataFrame
#
def emptyframe(**kwargs):
    # set default keyword arguments
    kwargs.setdefault('crs',DEFAULT_CRS)
    return geopandas.GeoDataFrame(geometry=geopandas.points_from_xy([], [], []), crs=kwargs['crs'])

#
# Get Values from Raw Buffer
#
def getvalues(data, dtype, size, num_elements=0):
    """
    data:   tuple of bytes
    dtype:  element of codedtype
    size:   bytes in data
    """
    raw = bytes(data)
    datatype = BASIC_TYPES[CODED_TYPE[dtype]]["nptype"]
    if num_elements == 0: # dynamically determine number of elements
        num_elements = int(size / numpy.dtype(datatype).itemsize)
    slicesize = num_elements * numpy.dtype(datatype).itemsize # truncates partial bytes
    values = numpy.frombuffer(raw[:slicesize], dtype=datatype, count=num_elements)
    return values

#
# Process Output File
#
def procoutputfile(parm, rsps):
    '''
    parm: request parameters
    rsps: response
    '''
    output = parm["output"]

    # Get file path to read from
    if "path" in output:
        path = output["path"]
    else:
        path = None

    # Check if it is a remote path
    for rsp in rsps:
        if 'arrowrec.remote' == rsp['__rectype']:
            path = rsp['url']
            break

    # Handle local files
    local_file = path # default to just returning the name of the parquet file
    if "open_on_complete" in output and output["open_on_complete"]:

        # Open the file as a DataFrame
        if (output["format"] == "geoparquet") or (output["format"] == "parquet" and "as_geo" in output and output["as_geo"]):
            local_file = geopandas.read_parquet(path) # GeoParquet
        elif output["format"] == "parquet":
            local_file = geopandas.pd.read_parquet(path) # Parquet
        elif output["format"] == "feather":
            local_file = geopandas.pd.read_feather(path) # Feather
        elif output["format"] == "csv":
            local_file = geopandas.pd.read_csv(path) # CSV
        else:
            raise FatalError('unsupported output format: %s' % (output["format"]))

        # Read metadata from file
        try:
            # Imports needed just to read metadata
            import pyarrow.parquet as pq
            import ctypes
            import json
            # pull out metadata
            metadata = pq.read_metadata(path)
            sliderule_metadata = ctypes.create_string_buffer(metadata.metadata[b'sliderule']).value.decode('ascii')
            local_file.attrs['sliderule'] = json.loads(sliderule_metadata)
            recordinfo_metadata = ctypes.create_string_buffer(metadata.metadata[b'recordinfo']).value.decode('ascii')
            local_file.attrs['recordinfo'] = json.loads(recordinfo_metadata)
            meta_metadata = ctypes.create_string_buffer(metadata.metadata[b'meta']).value.decode('ascii')
            local_file.attrs['meta'] = json.loads(meta_metadata)
        except Exception as e:
            # could fail for a multitude of reasons; just log and move on
            logger.debug(f'Failed to read metadata from {path}: {e}')

    # Return back to caller either path or opened dataframe
    return local_file

#
# Dictionary to GeoDataFrame
#
def todataframe(columns, time_key="time", lon_key="longitude", lat_key="latitude", height_key=None, **kwargs):

    # Set Default Keyword Arguments
    kwargs.setdefault('index_key','time')
    kwargs.setdefault('crs',DEFAULT_CRS)

    # Check Empty Columns
    if len(columns) <= 0:
        return emptyframe(**kwargs)

    # Generate Time Column
    columns['time'] = columns[time_key].astype('datetime64[ns]')

    # Generate Geometry Column
    # 3D point geometry
    # This enables 3D CRS transformations using the to_crs() method
    if height_key == None:
        geometry = geopandas.points_from_xy(columns[lon_key], columns[lat_key])
    else:
        geometry = geopandas.points_from_xy(columns[lon_key], columns[lat_key], columns[height_key])
    del columns[lon_key]
    del columns[lat_key]

    # Create Pandas DataFrame object
    if type(columns) == dict:
        df = geopandas.pd.DataFrame(columns)
    else:
        df = columns

    # Build GeoDataFrame (default geometry is crs=SLIDERULE_EPSG)
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=kwargs['crs'])

    # Set index (default is Timestamp), can add `verify_integrity=True` to check for duplicates
    # Can do this during DataFrame creation, but this allows input argument for desired column
    gdf.set_index(kwargs['index_key'], inplace=True)

    # Sort values for reproducible output despite async processing
    gdf.sort_index(inplace=True)

    # Return GeoDataFrame
    return gdf

#
# Simplify Polygon
#
def simplifypolygon(parm, tolerance):
    if "parms" not in parm:
        return

    if "cmr" in parm["parms"]:
        polygon = parm["parms"]["cmr"]["polygon"]
    elif "poly" in parm["parms"]:
        polygon = parm["parms"]["poly"]
    else:
        return

    raw_multi_polygon = [[(tuple([(c['lon'], c['lat']) for c in polygon]), [])]]
    shape = MultiPolygon(*raw_multi_polygon)
    buffered_shape = shape.buffer(tolerance)
    simplified_shape = buffered_shape.simplify(tolerance)
    simplified_coords = list(simplified_shape.exterior.coords)

    simplified_polygon = []
    for coord in simplified_coords:
        point = {"lon": coord[0], "lat": coord[1]}
        simplified_polygon.insert(0, point)

    if "cmr" not in parm["parms"]:
        parm["parms"]["cmr"] = {}
    parm["parms"]["cmr"]["polygon"] = simplified_polygon

    logger.warning('Using simplified polygon (for CMR request only!), {} points using tolerance of {}'.format(len(simplified_coords), tolerance))

#
# Set Session
#
def checksession(session):
    global slideruleSession
    return session if session != None else slideruleSession
