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
import time
import itertools
import copy
import json
import ssl
import urllib.request
import datetime
import logging
import warnings
import numpy
import geopandas
from shapely.geometry.multipolygon import MultiPolygon
from shapely.geometry import Polygon
import sliderule

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# import cluster support
clustering_enabled = False
try:
    from sklearn.cluster import KMeans
    clustering_enabled = True
except:
    logger.warning("Unable to import sklearn... clustering support disabled")

# profiling times for each major function
profiles = {}

# default asset
DEFAULT_ASSET="nsidc-s3"

# default standard data product version
DEFAULT_ICESAT2_SDP_VERSION='005'

# default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300
max_requested_resources = DEFAULT_MAX_REQUESTED_RESOURCES

# default maximum number of workers used for one request
DEFAULT_MAX_WORKERS_PER_NODE = 3

# icesat2 parameters
CNF_POSSIBLE_TEP = -2
CNF_NOT_CONSIDERED = -1
CNF_BACKGROUND = 0
CNF_WITHIN_10M = 1
CNF_SURFACE_LOW = 2
CNF_SURFACE_MEDIUM = 3
CNF_SURFACE_HIGH = 4
SRT_LAND = 0
SRT_OCEAN = 1
SRT_SEA_ICE = 2
SRT_LAND_ICE = 3
SRT_INLAND_WATER = 4
ALL_ROWS = -1
MAX_COORDS_IN_POLYGON = 16384
GT1L = 10
GT1R = 20
GT2L = 30
GT2R = 40
GT3L = 50
GT3R = 60
STRONG_SPOTS = (1, 3, 5)
WEAK_SPOTS = (2, 4, 6)
LEFT_PAIR = 0
RIGHT_PAIR = 1
SC_BACKWARD = 0
SC_FORWARD = 1

# gps-based epoch for delta times
ATLAS_SDP_EPOCH = datetime.datetime(2018, 1, 1)

###############################################################################
# NSIDC UTILITIES
###############################################################################
# The functions below have been adapted from the NSIDC download script and
# carry the following notice:
#
# Copyright (c) 2020 Regents of the University of Colorado
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.

# WGS84 / Mercator, Earth as Geoid, Coordinate system on the surface of a sphere or ellipsoid of reference.
EPSG_MERCATOR = "EPSG:4326"

CMR_URL = 'https://cmr.earthdata.nasa.gov'
CMR_PAGE_SIZE = 2000
CMR_FILE_URL = ('{0}/search/granules.json?provider=NSIDC_ECS'
                '&sort_key[]=start_date&sort_key[]=producer_granule_id'
                '&scroll=true&page_size={1}'.format(CMR_URL, CMR_PAGE_SIZE))

def __build_version_query_params(version):
    desired_pad_length = 3
    if len(version) > desired_pad_length:
        raise RuntimeError('Version string too long: "{0}"'.format(version))

    version = str(int(version))  # Strip off any leading zeros
    query_params = ''

    while len(version) <= desired_pad_length:
        padded_version = version.zfill(desired_pad_length)
        query_params += '&version={0}'.format(padded_version)
        desired_pad_length -= 1
    return query_params

def __cmr_filter_urls(search_results):
    """Select only the desired data files from CMR response."""
    if 'feed' not in search_results or 'entry' not in search_results['feed']:
        return []

    entries = [e['links']
               for e in search_results['feed']['entry']
               if 'links' in e]
    # Flatten "entries" to a simple list of links
    links = list(itertools.chain(*entries))

    urls = []
    unique_filenames = set()
    for link in links:
        if 'href' not in link:
            # Exclude links with nothing to download
            continue
        if 'inherited' in link and link['inherited'] is True:
            # Why are we excluding these links?
            continue
        if 'rel' in link and 'data#' not in link['rel']:
            # Exclude links which are not classified by CMR as "data" or "metadata"
            continue

        if 'title' in link and 'opendap' in link['title'].lower():
            # Exclude OPeNDAP links--they are responsible for many duplicates
            # This is a hack; when the metadata is updated to properly identify
            # non-datapool links, we should be able to do this in a non-hack way
            continue

        filename = link['href'].split('/')[-1]
        if filename in unique_filenames:
            # Exclude links with duplicate filenames (they would overwrite)
            continue

        unique_filenames.add(filename)

        if ".h5" in link['href'][-3:]:
            resource = link['href'].split("/")[-1]
            urls.append(resource)

    return urls

def __cmr_granule_metadata(search_results):
    """Get the metadata for CMR returned granules"""
    # GeoDataFrame with granule metadata
    granule_metadata = __emptyframe()
    # return empty dataframe if no CMR entries
    if 'feed' not in search_results or 'entry' not in search_results['feed']:
        return granule_metadata
    # for each CMR entry
    for e in search_results['feed']['entry']:
        # columns for dataframe
        columns = {}
        # time start and time end of granule
        columns['time_start'] = numpy.datetime64(e['time_start'])
        columns['time_end'] = numpy.datetime64(e['time_end'])
        columns['time_updated'] = numpy.datetime64(e['updated'])
        # get the granule size and convert to bits
        columns['granule_size'] = float(e['granule_size'])*(2.0**20)
        # Create Pandas DataFrame object
        # use granule id as index
        df = geopandas.pd.DataFrame(columns, index=[e['id']])
        # Generate Geometry Column
        if 'polygons' in e:
            coords = [float(i) for i in e['polygons'][0][0].split()]
            geometry = Polygon(zip(coords[1::2], coords[::2]))
        else:
            geometry, = geopandas.points_from_xy([None], [None])
        # Build GeoDataFrame (default geometry is crs=EPSG_MERCATOR)
        gdf = geopandas.GeoDataFrame(df, geometry=[geometry], crs=EPSG_MERCATOR)
        # append to combined GeoDataFrame and catch warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            granule_metadata = granule_metadata.append(gdf)
    # return granule metadata
    # - time start and time end
    # - time granule was updated
    # - granule size in bits
    # - polygons as geodataframe geometry
    return granule_metadata

def __cmr_search(short_name, version, time_start, time_end, **kwargs):
    """Perform a scrolling CMR query for files matching input criteria."""
    kwargs.setdefault('polygon',None)
    kwargs.setdefault('name_filter',None)
    kwargs.setdefault('return_metadata',False)
    # build params
    params = '&short_name={0}'.format(short_name)
    params += __build_version_query_params(version)
    params += '&temporal[]={0},{1}'.format(time_start, time_end)
    if kwargs['polygon']:
        params += '&polygon={0}'.format(kwargs['polygon'])
    if kwargs['name_filter']:
        params += '&options[producer_granule_id][pattern]=true'
        params += '&producer_granule_id[]=' + kwargs['name_filter']
    cmr_query_url = CMR_FILE_URL + params
    logger.debug('cmr request={0}\n'.format(cmr_query_url))

    cmr_scroll_id = None
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    urls = []
    # GeoDataFrame with granule metadata
    metadata = __emptyframe()
    while True:
        req = urllib.request.Request(cmr_query_url)
        if cmr_scroll_id:
            req.add_header('cmr-scroll-id', cmr_scroll_id)
        response = urllib.request.urlopen(req, context=ctx)
        if not cmr_scroll_id:
            # Python 2 and 3 have different case for the http headers
            headers = {k.lower(): v for k, v in dict(response.info()).items()}
            cmr_scroll_id = headers['cmr-scroll-id']
            hits = int(headers['cmr-hits'])
        search_page = response.read()
        search_page = json.loads(search_page.decode('utf-8'))
        url_scroll_results = __cmr_filter_urls(search_page)
        if not url_scroll_results:
            break
        urls += url_scroll_results
        # query for granule metadata and polygons
        if kwargs['return_metadata']:
            metadata_results = __cmr_granule_metadata(search_page)
        else:
            metadata_results = [None for _ in url_scroll_results]
        # append granule metadata and catch warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            metadata = metadata.append(metadata_results)

    return (urls,metadata)

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
# Calculate Laser Spot
#
def __calcspot(sc_orient, track, pair):

    # spacecraft in forward orientation
    if sc_orient == SC_BACKWARD:
        if track == 1:
            if pair == LEFT_PAIR:
                return 1
            elif pair == RIGHT_PAIR:
                return 2
        elif track == 2:
            if pair == LEFT_PAIR:
                return 3
            elif pair == RIGHT_PAIR:
                return 4
        elif track == 3:
            if pair == LEFT_PAIR:
                return 5
            elif pair == RIGHT_PAIR:
                return 6

    # spacecraft in backward orientation
    elif sc_orient == SC_FORWARD:
        if track == 1:
            if pair == LEFT_PAIR:
                return 6
            elif pair == RIGHT_PAIR:
                return 5
        elif track == 2:
            if pair == LEFT_PAIR:
                return 4
            elif pair == RIGHT_PAIR:
                return 3
        elif track == 3:
            if pair == LEFT_PAIR:
                return 2
            elif pair == RIGHT_PAIR:
                return 1

    # unknown spot
    return 0

#
#  Get Values from Raw Buffer
#
def __get_values(data, dtype, size):
    """
    data:   tuple of bytes
    dtype:  element of codedtype
    size:   bytes in data
    """

    raw = bytes(data)
    datatype = sliderule.basictypes[sliderule.codedtype2str[dtype]]["nptype"]
    num_elements = int(size / numpy.dtype(datatype).itemsize)
    slicesize = num_elements * numpy.dtype(datatype).itemsize # truncates partial bytes
    values = numpy.frombuffer(raw[:slicesize], dtype=datatype, count=num_elements)

    return values

#
#  Query Resources from CMR
#
def __query_resources(parm, version, **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Check Parameters are Valid
    if ("poly" not in parm) and ("t0" not in parm) and ("t1" not in parm):
        logger.error("Must supply some bounding parameters with request (poly, t0, t1)")
        return []

    # Submission Arguments for CMR
    kwargs['version'] = version
    kwargs.setdefault('return_metadata', False)

    # Pull Out Polygon
    if "clusters" in parm and parm["clusters"] and len(parm["clusters"]) > 0:
        kwargs['polygon'] = parm["clusters"]
    elif "poly" in parm and parm["poly"] and len(parm["poly"]) > 0:
        kwargs['polygon'] = parm["poly"]

    # Pull Out Time Period
    if "t0" in parm:
        kwargs['time_start'] = parm["t0"]
    if "t1" in parm:
        kwargs['time_end'] = parm["t1"]

    # Build Filters
    name_filter_enabled = False
    rgt_filter = '????'
    if "rgt" in parm:
        rgt_filter = f'{parm["rgt"]}'.zfill(4)
        name_filter_enabled = True
    cycle_filter = '??'
    if "cycle" in parm:
        cycle_filter = f'{parm["cycle"]}'.zfill(2)
        name_filter_enabled = True
    region_filter = '??'
    if "region" in parm:
        region_filter = f'{parm["region"]}'.zfill(2)
        name_filter_enabled = True
    if name_filter_enabled:
        kwargs['name_filter'] = '*_' + rgt_filter + cycle_filter + region_filter + '_*'

    # Make CMR Request
    if kwargs['return_metadata']:
        resources,metadata = cmr(**kwargs)
    else:
        resources = cmr(**kwargs)

    # Check Resources are Under Limit
    if(len(resources) > max_requested_resources):
        raise RuntimeError('Exceeded maximum requested granules: {} (current max is {})\nConsider using icesat2.set_max_resources to set a higher limit.'.format(len(resources), max_requested_resources))
    else:
        logger.info("Identified %d resources to process", len(resources))

    # Update Profile
    profiles[__query_resources.__name__] = time.perf_counter() - tstart

    # Return Resources
    if kwargs['return_metadata']:
        return (resources,metadata)
    else:
        return resources

#
#  Create Empty GeoDataFrame
#
def __emptyframe(**kwargs):
    # set default keyword arguments
    kwargs['crs'] = EPSG_MERCATOR
    return geopandas.GeoDataFrame(geometry=geopandas.points_from_xy([], []), crs=kwargs['crs'])

#
#  Dictionary to GeoDataFrame
#
def __todataframe(columns, delta_time_key="delta_time", lon_key="lon", lat_key="lat", **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Set Default Keyword Arguments
    kwargs['index_key'] = "time"
    kwargs['crs'] = EPSG_MERCATOR

    # Check Empty Columns
    if len(columns) <= 0:
        return __emptyframe(**kwargs)

    # Generate Time Column
    delta_time = (columns[delta_time_key]*1e9).astype('timedelta64[ns]')
    atlas_sdp_epoch = numpy.datetime64(ATLAS_SDP_EPOCH)
    columns['time'] = geopandas.pd.to_datetime(atlas_sdp_epoch + delta_time)

    # Generate Geometry Column
    geometry = geopandas.points_from_xy(columns[lon_key], columns[lat_key])
    del columns[lon_key]
    del columns[lat_key]

    # Create Pandas DataFrame object
    if type(columns) == dict:
        df = geopandas.pd.DataFrame(columns)
    else:
        df = columns

    # Build GeoDataFrame (default geometry is crs=EPSG_MERCATOR)
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=kwargs['crs'])

    # Set index (default is Timestamp), can add `verify_integrity=True` to check for duplicates
    # Can do this during DataFrame creation, but this allows input argument for desired column
    gdf.set_index(kwargs['index_key'], inplace=True)

    # Sort values for reproducible output despite async processing
    gdf.sort_index(inplace=True)

    # Update Profile
    profiles[__todataframe.__name__] = time.perf_counter() - tstart

    # Return GeoDataFrame
    return gdf

#
# GeoDataFrame to Polygon
#
def __gdf2poly(gdf):

    # latch start time
    tstart = time.perf_counter()

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

    # Update Profile
    profiles[__gdf2poly.__name__] = time.perf_counter() - tstart

    # return polygon
    return polygon

#
# Process Output File
#
def __procoutputfile(parm, lon_key, lat_key):
    if "open_on_complete" in parm["output"] and parm["output"]["open_on_complete"]:
        # Return GeoParquet File as GeoDataFrame
        return geopandas.read_parquet(parm["output"]["path"])
    else:
        # Return Parquet Filename
        return parm["output"]["path"]

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url, verbose=False, max_resources=DEFAULT_MAX_REQUESTED_RESOURCES, loglevel=logging.CRITICAL, organization=sliderule.service_org):
    '''
    Initializes the Python client for use with SlideRule, and should be called before other ICESat-2 API calls.
    This function is a wrapper for a handful of sliderule functions that would otherwise all have to be called in order to initialize the client.

    Parameters
    ----------
        url :           str
                        the IP address or hostname of the SlideRule service (note, there is a special case where the url is provided as a list of strings instead of just a string; when a list is provided, the client hardcodes the set of servers that are used to process requests to the exact set provided; this is used for testing and for local installations and can be ignored by most users)
        verbose :       bool
                        whether or not user level log messages received from SlideRule generate a Python log message (see `sliderule.set_verbose <../user_guide/SlideRule.html#set_verbose>`_)
        max_resources : int
                        the maximum number of resources that are allowed to be processed in a single request
        loglevel :      int
                        minimum severity of log message to output
        organization:   str
                        SlideRule provisioning system organization the user belongs to (see sliderule.authenticate for details)

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init("my-sliderule-service.my-company.com", True)
    '''
    set_max_resources(max_resources)
    if verbose:
        loglevel = logging.INFO
    logging.basicConfig(level=loglevel)
    sliderule.set_url(url)
    sliderule.set_verbose(verbose)
    sliderule.authenticate(organization)
    sliderule.check_version(plugins=['icesat2'])

#
#  Set Maximum Resources
#
def set_max_resources (max_resources):
    '''
    Sets the maximum allowed number of resources to be processed in one request.  This is mainly provided as a sanity check for the user.

    Parameters
    ----------
        max_resources : int
                        the maximum number of resources that are allowed to be processed in a single request

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.set_max_resources(1000)
    '''
    global max_requested_resources
    max_requested_resources = max_resources

#
#  Common Metadata Repository
#
def cmr(**kwargs):
    '''
    Query the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov/search>`_ for a list of data within temporal and spatial parameters

    Parameters
    ----------
        polygon:    list
                    either a single list of longitude,latitude in counter-clockwise order with first and last point matching, defining region of interest (see `polygons <../user_guide/ICESat-2.html#polygons>`_), or a list of such lists when the region includes more than one polygon
        time_start: str
                    starting time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        time_end:   str
                    ending time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        version:    str
                    dataset version as found in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
        short_name: str
                    dataset short name as defined in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_

    Returns
    -------
    list
        files (granules) for the dataset fitting the spatial and temporal parameters

    Examples
    --------
        >>> from sliderule import icesat2
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033},
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> granules = icesat2.cmr(polygon=region)
        >>> granules
        ['ATL03_20181017222812_02950102_003_01.h5', 'ATL03_20181110092841_06530106_003_01.h5', ... 'ATL03_20201111102237_07370902_003_01.h5']
    '''
    # set default polygon
    kwargs.setdefault('polygon', None)
    # set default start time to start of ICESat-2 mission
    kwargs.setdefault('time_start', '2018-10-13T00:00:00Z')
    # set default stop time to current time
    kwargs.setdefault('time_end', datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"))
    # set default version and product short name
    kwargs.setdefault('version', DEFAULT_ICESAT2_SDP_VERSION)
    kwargs.setdefault('short_name','ATL03')
    # return metadata for each requested granule
    kwargs.setdefault('return_metadata', False)
    # set default name filter
    kwargs.setdefault('name_filter', None)

    # return value
    resources = {} # [<url>] = <polygon>

    # create list of polygons
    polygons = [None]
    if kwargs['polygon'] and len(kwargs['polygon']) > 0:
        if type(kwargs['polygon'][0]) == dict:
            polygons = [copy.deepcopy(kwargs['polygon'])]
        elif type(kwargs['polygon'][0] == list):
            polygons = copy.deepcopy(kwargs['polygon'])

    # iterate through each polygon (or none if none supplied)
    for polygon in polygons:
        urls = []
        metadata = __emptyframe()

        # issue CMR request
        for tolerance in [0.0001, 0.001, 0.01, 0.1, 1.0, None]:

            # convert polygon list into string
            polystr = None
            if polygon:
                flatpoly = []
                for p in polygon:
                    flatpoly.append(p["lon"])
                    flatpoly.append(p["lat"])
                polystr = str(flatpoly)[1:-1]
                polystr = polystr.replace(" ", "") # remove all spaces as this will be embedded in a url

            # call into NSIDC routines to make CMR request
            try:
                urls,metadata = __cmr_search(kwargs['short_name'],
                                            kwargs['version'],
                                            kwargs['time_start'],
                                            kwargs['time_end'],
                                            polygon=polystr,
                                            return_metadata=kwargs['return_metadata'],
                                            name_filter=kwargs['name_filter'])
                break # exit loop because cmr search was successful
            except urllib.error.HTTPError as e:
                logger.error('HTTP Request Error: {}'.format(e.reason))
            except RuntimeError as e:
                logger.error("Runtime Error:", e)

            # simplify polygon
            if polygon and tolerance:
                raw_multi_polygon = [[(tuple([(c['lon'], c['lat']) for c in polygon]), [])]]
                shape = MultiPolygon(*raw_multi_polygon)
                buffered_shape = shape.buffer(tolerance)
                simplified_shape = buffered_shape.simplify(tolerance)
                simplified_coords = list(simplified_shape.exterior.coords)
                logger.warning('Using simplified polygon (for CMR request only!), {} points using tolerance of {}'.format(len(simplified_coords), tolerance))
                region = []
                for coord in simplified_coords:
                    point = {"lon": coord[0], "lat": coord[1]}
                    region.insert(0,point)
                polygon = region
            else:
                break # exit here because nothing can be done

        # populate resources
        for i,url, in enumerate(urls):
            resources[url] = metadata.iloc[i]

    # build return lists
    url_list = list(resources.keys())
    meta_list = list(resources.values())

    if kwargs['return_metadata']:
        return (url_list,meta_list)
    else:
        return url_list

#
#  ATL06
#
def atl06 (parm, resource, asset=DEFAULT_ASSET):
    '''
    Performs ATL06-SR processing on ATL03 data and returns gridded elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure ATL06-SR algorithm processing (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
    resource:   str
                ATL03 HDF5 filename
    asset:      str
                data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        gridded elevations (see `Elevations <../user_guide/ICESat-2.html#elevations>`_)
    '''
    return atl06p(parm, asset=asset, resources=[resource])

#
#  Parallel ATL06
#
def atl06p(parm, asset=DEFAULT_ASSET, version=DEFAULT_ICESAT2_SDP_VERSION, callbacks={}, resources=None):
    '''
    Performs ATL06-SR processing in parallel on ATL03 data and returns gridded elevations.  This function expects that the **parm** argument
    includes a polygon which is used to fetch all available resources from the CMR system automatically.  If **resources** is specified
    then any polygon or resource filtering options supplied in **parm** are ignored.

    Warnings
    --------
        It is often the case that the list of resources (i.e. granules) returned by the CMR system includes granules that come close, but
        do not actually intersect the region of interest.  This is due to geolocation margin added to all CMR ICESat-2 resources in order to account
        for the spacecraft off-pointing.  The consequence is that SlideRule will return no data for some of the resources and issue a warning statement
        to that effect; this can be ignored and indicates no issue with the data processing.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure ATL06-SR algorithm processing (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
        asset:          str
                        data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)
        version:        str
                        the version of the ATL03 data to use for processing
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_004_01.h5", ...])

    Returns
    -------
    GeoDataFrame
        gridded elevations (see `Elevations <../user_guide/ICESat-2.html#elevations>`_)

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init("slideruleearth.io", True)
        >>> parms = { "cnf": 4, "ats": 20.0, "cnt": 10, "len": 40.0, "res": 20.0, "maxi": 1 }
        >>> resources = ["ATL03_20181019065445_03150111_003_01.h5"]
        >>> atl03_asset = "atlas-local"
        >>> rsps = icesat2.atl06p(parms, asset=atl03_asset, resources=resources)
        >>> rsps
                dh_fit_dx  w_surface_window_final  ...                       time                     geometry
        0        0.000042               61.157661  ... 2018-10-19 06:54:46.104937  POINT (-63.82088 -79.00266)
        1        0.002019               61.157683  ... 2018-10-19 06:54:46.467038  POINT (-63.82591 -79.00247)
        2        0.001783               61.157678  ... 2018-10-19 06:54:46.107756  POINT (-63.82106 -79.00283)
        3        0.000969               61.157666  ... 2018-10-19 06:54:46.469867  POINT (-63.82610 -79.00264)
        4       -0.000801               61.157665  ... 2018-10-19 06:54:46.110574  POINT (-63.82124 -79.00301)
        ...           ...                     ...  ...                        ...                          ...
        622407  -0.000970               61.157666  ... 2018-10-19 07:00:29.606632  POINT (135.57522 -78.98983)
        622408   0.004620               61.157775  ... 2018-10-19 07:00:29.250312  POINT (135.57052 -78.98983)
        622409  -0.001366               61.157671  ... 2018-10-19 07:00:29.609435  POINT (135.57504 -78.98966)
        622410  -0.004041               61.157748  ... 2018-10-19 07:00:29.253123  POINT (135.57034 -78.98966)
        622411  -0.000482               61.157663  ... 2018-10-19 07:00:29.612238  POINT (135.57485 -78.98948)

        [622412 rows x 16 columns]
    '''
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not supplied)
        if resources == None:
            resources = __query_resources(parm, version)

        # Build ATL06 Request
        rqst = {
            "atl03-asset" : asset,
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source("atl06p", rqst, stream=True, callbacks=callbacks)

        # Check for Output Options
        if "output" in parm:
            profiles[atl06p.__name__] = time.perf_counter() - tstart
            return __procoutputfile(parm, "lon", "lat")
        else: # Native Output
            # Flatten Responses
            tstart_flatten = time.perf_counter()
            columns = {}
            elevation_records = []
            num_elevations = 0
            field_dictionary = {} # [<field_name>] = {"extent_id": [], <field_name>: []}
            if len(rsps) > 0:
                # Sort Records
                for rsp in rsps:
                    if 'atl06rec' in rsp['__rectype']:
                        elevation_records += rsp,
                        num_elevations += len(rsp['elevation'])
                    elif 'extrec' == rsp['__rectype']:
                        field_name = parm['atl03_geo_fields'][rsp['field_index']]
                        if field_name not in field_dictionary:
                            field_dictionary[field_name] = {'extent_id': [], field_name: []}
                        # Parse Ancillary Data
                        data = __get_values(rsp['data'], rsp['datatype'], len(rsp['data']))
                        # Add Left Pair Track Entry
                        field_dictionary[field_name]['extent_id'] += rsp['extent_id'] | 0x2,
                        field_dictionary[field_name][field_name] += data[LEFT_PAIR],
                        # Add Right Pair Track Entry
                        field_dictionary[field_name]['extent_id'] += rsp['extent_id'] | 0x3,
                        field_dictionary[field_name][field_name] += data[RIGHT_PAIR],
                    elif 'rsrec' == rsp['__rectype']:
                        for sample in rsp["samples"]:
                            time_str = sliderule.gps2utc(sample["time"])
                            field_name = parm['samples'][rsp['raster_index']] + "-" + time_str.split(" ")[0].strip()
                            if field_name not in field_dictionary:
                                field_dictionary[field_name] = {'extent_id': [], field_name: []}
                            field_dictionary[field_name]['extent_id'] += rsp['extent_id'],
                            field_dictionary[field_name][field_name] += sample['value'],
                # Build Elevation Columns
                if num_elevations > 0:
                    # Initialize Columns
                    sample_elevation_record = elevation_records[0]["elevation"][0]
                    for field in sample_elevation_record.keys():
                        fielddef = sliderule.get_definition(sample_elevation_record['__rectype'], field)
                        if len(fielddef) > 0:
                            columns[field] = numpy.empty(num_elevations, fielddef["nptype"])
                    # Populate Columns
                    elev_cnt = 0
                    for record in elevation_records:
                        for elevation in record["elevation"]:
                            for field in columns:
                                columns[field][elev_cnt] = elevation[field]
                            elev_cnt += 1
            else:
                logger.debug("No response returned")

            profiles["flatten"] = time.perf_counter() - tstart_flatten

            # Build GeoDataFrame
            gdf = __todataframe(columns)

            # Merge Ancillary Fields
            tstart_merge = time.perf_counter()
            for field in field_dictionary:
                df = geopandas.pd.DataFrame(field_dictionary[field])
                gdf = geopandas.pd.merge(gdf, df, on='extent_id', how='left').set_axis(gdf.index)
            profiles["merge"] = time.perf_counter() - tstart_merge

            # Delete Extent ID Column
            if len(gdf) > 0:
                del gdf["extent_id"]

            # Return Response
            profiles[atl06p.__name__] = time.perf_counter() - tstart
            return gdf

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return __emptyframe()

#
#  Subsetted ATL03
#
def atl03s (parm, resource, asset=DEFAULT_ASSET):
    '''
    Subsets ATL03 data given the polygon and time range provided and returns segments of photons

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
        resource:   str
                    ATL03 HDF5 filename
        asset:      str
                    data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        ATL03 extents (see `Photon Segments <../user_guide/ICESat-2.html#photon-segments>`_)
    '''
    return atl03sp(parm, asset=asset, resources=[resource])

#
#  Parallel Subsetted ATL03
#
def atl03sp(parm, asset=DEFAULT_ASSET, version=DEFAULT_ICESAT2_SDP_VERSION, callbacks={}, resources=None):
    '''
    Performs ATL03 subsetting in parallel on ATL03 data and returns photon segment data.  Unlike the `atl03s <#atl03s>`_ function,
    this function does not take a resource as a parameter; instead it is expected that the **parm** argument includes a polygon which
    is used to fetch all available resources from the CMR system automatically.

    Warnings
    --------
        Note, it is often the case that the list of resources (i.e. granules) returned by the CMR system includes granules that come close, but
        do not actually intersect the region of interest.  This is due to geolocation margin added to all CMR ICESat-2 resources in order to account
        for the spacecraft off-pointing.  The consequence is that SlideRule will return no data for some of the resources and issue a warning statement to that effect; this can be ignored and indicates no issue with the data processing.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure ATL03 subsetting (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
        asset:          str
                        data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)
        version:        str
                        the version of the ATL03 data to return
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_004_01.h5", ...])

    Returns
    -------
    GeoDataFrame
        ATL03 segments (see `Photon Segments <../user_guide/ICESat-2.html#photon-segments>`_)
    '''
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not specified)
        if resources == None:
            resources = __query_resources(parm, version)

        # Build ATL03 Subsetting Request
        rqst = {
            "atl03-asset" : asset,
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source("atl03sp", rqst, stream=True, callbacks=callbacks)

        # Check for Output Options
        if "output" in parm:
            profiles[atl03sp.__name__] = time.perf_counter() - tstart
            return __procoutputfile(parm, "longitude", "latitude")
        else: # Native Output
            # Flatten Responses
            tstart_flatten = time.perf_counter()
            columns = {}
            sample_photon_record = None
            photon_records = []
            num_photons = 0
            extent_dictionary = {}
            extent_field_types = {} # ['field_name'] = nptype
            photon_dictionary = {}
            photon_field_types = {} # ['field_name'] = nptype
            if len(rsps) > 0:
                # Sort Records
                for rsp in rsps:
                    extent_id = rsp['extent_id']
                    if 'atl03rec' in rsp['__rectype']:
                        photon_records += rsp,
                        num_photons += len(rsp['data'])
                        if sample_photon_record == None and len(rsp['data']) > 0:
                            sample_photon_record = rsp
                    elif 'extrec' == rsp['__rectype']:
                        # Get Field Type
                        field_name = parm['atl03_geo_fields'][rsp['field_index']]
                        if field_name not in extent_field_types:
                            extent_field_types[field_name] = sliderule.basictypes[sliderule.codedtype2str[rsp['datatype']]]["nptype"]
                        # Initialize Extent Dictionary Entry
                        if extent_id not in extent_dictionary:
                            extent_dictionary[extent_id] = {}
                        # Save of Values per Extent ID per Field Name
                        data = __get_values(rsp['data'], rsp['datatype'], len(rsp['data']))
                        extent_dictionary[extent_id][field_name] = data
                    elif 'phrec' == rsp['__rectype']:
                        # Get Field Type
                        field_name = parm['atl03_ph_fields'][rsp['field_index']]
                        if field_name not in photon_field_types:
                            photon_field_types[field_name] = sliderule.basictypes[sliderule.codedtype2str[rsp['datatype']]]["nptype"]
                        # Initialize Extent Dictionary Entry
                        if extent_id not in photon_dictionary:
                            photon_dictionary[extent_id] = {}
                        # Save of Values per Extent ID per Field Name
                        data = __get_values(rsp['data'], rsp['datatype'], len(rsp['data']))
                        photon_dictionary[extent_id][field_name] = data
                # Build Elevation Columns
                if num_photons > 0:
                    # Initialize Columns
                    for field in sample_photon_record.keys():
                        fielddef = sliderule.get_definition("atl03rec", field)
                        if len(fielddef) > 0:
                            columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                    for field in sample_photon_record["data"][0].keys():
                        fielddef = sliderule.get_definition("atl03rec.photons", field)
                        if len(fielddef) > 0:
                            columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                    for field in extent_field_types.keys():
                        columns[field] = numpy.empty(num_photons, extent_field_types[field])
                    for field in photon_field_types.keys():
                        columns[field] = numpy.empty(num_photons, photon_field_types[field])
                    # Populate Columns
                    ph_cnt = 0
                    for record in photon_records:
                        ph_index = 0
                        pair = 0
                        left_cnt = record["count"][0]
                        extent_id = record['extent_id']
                        # Get Extent Fields to Add to Extent
                        extent_field_dictionary = {}
                        if extent_id in extent_dictionary:
                            extent_field_dictionary = extent_dictionary[extent_id]
                        # Get Photon Fields to Add to Extent
                        photon_field_dictionary = {}
                        if extent_id in photon_dictionary:
                            photon_field_dictionary = photon_dictionary[extent_id]
                        # For Each Photon in Extent
                        for photon in record["data"]:
                            if ph_index >= left_cnt:
                                pair = 1
                            # Add per Extent Fields
                            for field in record.keys():
                                if field in columns:
                                    if field == "count":
                                        columns[field][ph_cnt] = pair # count gets changed to pair id
                                    elif type(record[field]) is tuple:
                                        columns[field][ph_cnt] = record[field][pair]
                                    else:
                                        columns[field][ph_cnt] = record[field]
                            # Add per Photon Fields
                            for field in photon.keys():
                                if field in columns:
                                    columns[field][ph_cnt] = photon[field]
                            # Add Ancillary Extent Fields
                            for field in extent_field_dictionary:
                                columns[field][ph_cnt] = extent_field_dictionary[field][pair]
                            # Add Ancillary Extent Fields
                            for field in photon_field_dictionary:
                                columns[field][ph_cnt] = photon_field_dictionary[field][ph_index]
                            # Goto Next Photon
                            ph_cnt += 1
                            ph_index += 1
                    # Rename Count Column to Pair Column
                    columns["pair"] = columns.pop("count")

                    # Delete Extent ID Column
                    if "extent_id" in columns:
                        del columns["extent_id"]

                    # Capture Time to Flatten
                    profiles["flatten"] = time.perf_counter() - tstart_flatten

                    # Create DataFrame
                    gdf = __todataframe(columns, lat_key="latitude", lon_key="longitude")

                    # Calculate Spot Column
                    gdf['spot'] = gdf.apply(lambda row: __calcspot(row["sc_orient"], row["track"], row["pair"]), axis=1)

                    # Return Response
                    profiles[atl03sp.__name__] = time.perf_counter() - tstart
                    return gdf
                else:
                    logger.debug("No photons returned")
            else:
                logger.debug("No response returned")

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)

    # Error or No Data
    return __emptyframe()

#
#  H5
#
def h5 (dataset, resource, asset=DEFAULT_ASSET, datatype=sliderule.datatypes["DYNAMIC"], col=0, startrow=0, numrows=ALL_ROWS):
    '''
    Reads a dataset from an HDF5 file and returns the values of the dataset in a list

    This function provides an easy way for locally run scripts to get direct access to HDF5 data stored in a cloud environment.
    But it should be noted that this method is not the most efficient way to access remote H5 data, as the data is accessed one dataset at a time.
    The ``h5p`` api is the preferred solution for reading multiple datasets.

    One of the difficulties in reading HDF5 data directly from a Python script is converting the format of the data as it is stored in HDF5 to a data
    format that is easy to use in Python.  The compromise that this function takes is that it allows the user to supply the desired data type of the
    returned data via the **datatype** parameter, and the function will then return a **numpy** array of values with that data type.

    The data type is supplied as a ``sliderule.datatypes`` enumeration:

    - ``sliderule.datatypes["TEXT"]``: return the data as a string of unconverted bytes
    - ``sliderule.datatypes["INTEGER"]``: return the data as an array of integers
    - ``sliderule.datatypes["REAL"]``: return the data as an array of double precision floating point numbers
    - ``sliderule.datatypes["DYNAMIC"]``: return the data in the numpy data type that is the closest match to the data as it is stored in the HDF5 file

    Parameters
    ----------
        dataset:    str
                    full path to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``)
        resource:   str
                    HDF5 filename
        asset:      str
                    data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)
        datatype:   int
                    the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
        col:        int
                    the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
        startrow:   int
                    the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
        numrows:    int
                    the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

    Returns
    -------
    numpy array
        dataset values

    Examples
    --------
        >>> segments    = icesat2.h5("/gt1r/land_ice_segments/segment_id",  resource, asset)
        >>> heights     = icesat2.h5("/gt1r/land_ice_segments/h_li",        resource, asset)
        >>> latitudes   = icesat2.h5("/gt1r/land_ice_segments/latitude",    resource, asset)
        >>> longitudes  = icesat2.h5("/gt1r/land_ice_segments/longitude",   resource, asset)
        >>> df = pd.DataFrame(data=list(zip(heights, latitudes, longitudes)), index=segments, columns=["h_mean", "latitude", "longitude"])
    '''
    tstart = time.perf_counter()
    datasets = [ { "dataset": dataset, "datatype": datatype, "col": col, "startrow": startrow, "numrows": numrows } ]
    values = h5p(datasets, resource, asset=asset)
    if len(values) > 0:
        profiles[h5.__name__] = time.perf_counter() - tstart
        return values[dataset]
    else:
        return numpy.empty(0)

#
#  Parallel H5
#
def h5p (datasets, resource, asset=DEFAULT_ASSET):
    '''
    Reads a list of datasets from an HDF5 file and returns the values of the dataset in a dictionary of lists.

    This function is considerably faster than the ``icesat2.h5`` function in that it not only reads the datasets in
    parallel on the server side, but also shares a file context between the reads so that portions of the file that
    need to be read multiple times do not result in multiple requests to S3.

    For a full discussion of the data type conversion options, see `h5 <../user_guide/ICESat-2.html#h5>`_.

    Parameters
    ----------
        datasets:   dict
                    list of full paths to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``); see below for additional parameters that can be added to each dataset
        resource:   str
                    HDF5 filename
        asset:      str
                    data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    dict
        numpy arrays of dataset values, where the keys are the dataset names

        The `datasets` dictionary can optionally contain the following elements per entry:

        * "valtype" (int): the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
        * "col" (int): the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
        * "startrow" (int): the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
        * "numrows" (int): the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init(["127.0.0.1"], False)
        >>> datasets = [
        ...         {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5}
        ...     ]
        >>> rsps = icesat2.h5p(datasets, "ATL06_20181019065445_03150111_003_01.h5", "atlas-local")
        >>> print(rsps)
        {'/gt2r/land_ice_segments/h_li': array([45.3146427 , 45.27640582, 45.23608027, 45.21131015, 45.15692304]),
         '/gt2l/land_ice_segments/h_li': array([45.35118977, 45.33535027, 45.27195617, 45.21816889, 45.18534204]),
         '/gt1l/land_ice_segments/h_li': array([45.68811156, 45.71368944, 45.74234326, 45.74614113, 45.79866465]),
         '/gt3l/land_ice_segments/h_li': array([45.29602321, 45.34764226, 45.31430979, 45.31471701, 45.30034622]),
         '/gt1r/land_ice_segments/h_li': array([45.72632446, 45.76512574, 45.76337375, 45.77102473, 45.81307948]),
         '/gt3r/land_ice_segments/h_li': array([45.14954134, 45.18970635, 45.16637644, 45.15235916, 45.17135806])}
    '''
    # Latch Start Time
    tstart = time.perf_counter()

    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "datasets": datasets,
    }

    # Read H5 File
    try:
        rsps = sliderule.source("h5p", rqst, stream=True)
    except RuntimeError as e:
        logger.critical(e)
        rsps = []

    # Build Record Data
    results = {}
    for result in rsps:
        results[result["dataset"]] = __get_values(result["data"], result["datatype"], result["size"])

    # Update Profiles
    profiles[h5p.__name__] = time.perf_counter() - tstart

    # Return Results
    return results

#
# Format Region Specification
#
def toregion(source, tolerance=0.0, cellsize=0.01, n_clusters=1):
    '''
    Convert a GeoJSON representation of a set of geospatial regions into a list of lat,lon coordinates and raster image recognized by SlideRule

    Parameters
    ----------
        filename:   str
                    file name of GeoJSON formatted regions of interest, file **must** have name with the .geojson suffix
                    file name of ESRI Shapefile formatted regions of interest, file **must** have name with the .shp suffix
        tolerance:  float
                    tolerance used to simplify complex shapes so that the number of points is less than the limit (a tolerance of 0.001 typically works for most complex shapes)
        cellsize:   float
                    size of pixel in degrees used to create the raster image of the polygon
        clusters:   int
                    number of clusters of polygons to create when breaking up the request to CMR

    Returns
    -------
    dict
        a list of longitudes and latitudes containing the region of interest that can be used for the **poly** and **raster** parameters in a processing request to SlideRule.

        region = {"poly": [{"lat": <lat1>, "lon": <lon1>, ... }], "clusters": [{"lat": <lat1>, "lon": <lon1>, ... }, {"lat": <lat1>, "lon": <lon1>, ... }, ...], "raster": {"data": <geojson file as string>, "length": <length of geojson file>, "cellsize": <parameter cellsize>}}

    Examples
    --------
        >>> from sliderule import icesat2
        >>> # Region of Interest #
        >>> region_filename = sys.argv[1]
        >>> region = icesat2.toregion(region_filename)
        >>> # Configure SlideRule #
        >>> icesat2.init("slideruleearth.io", False)
        >>> # Build ATL06 Request #
        >>> parms = {
        ...     "poly": region["poly"],
        ...     "srt": icesat2.SRT_LAND,
        ...     "cnf": icesat2.CNF_SURFACE_HIGH,
        ...     "ats": 10.0,
        ...     "cnt": 10,
        ...     "len": 40.0,
        ...     "res": 20.0,
        ...     "maxi": 1
        ... }
        >>> # Get ATL06 Elevations
        >>> atl06 = icesat2.atl06p(parms)
    '''

    tstart = time.perf_counter()
    tempfile = "temp.geojson"

    if isinstance(source, geopandas.GeoDataFrame):
        # user provided GeoDataFrame instead of a file
        gdf = source
        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, list) and (len(source) >= 4) and (len(source) % 2 == 0):
        # create lat/lon lists
        if len(source) == 4: # bounding box
            lons = [source[0], source[2], source[2], source[0], source[0]]
            lats = [source[1], source[1], source[3], source[3], source[1]]
        elif len(source) > 4: # polygon list
            lons = [source[i] for i in range(1,len(source),2)]
            lats = [source[i] for i in range(0,len(source),2)]

        # create geodataframe
        p = Polygon([point for point in zip(lons, lats)])
        gdf = geopandas.GeoDataFrame(geometry=[p], crs=EPSG_MERCATOR)

        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, str) and (source.find(".shp") > 1):
        # create geodataframe
        gdf = geopandas.read_file(source)
        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, str) and (source.find(".geojson") > 1):
        # create geodataframe
        gdf = geopandas.read_file(source)
        with open(source, mode='rt') as file:
            datafile = file.read()

    else:
        raise TypeError("incorrect filetype: please use a .geojson, .shp, or a geodataframe")


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
        polygon = __gdf2poly(gdf)

        # generate clusters
        clusters = []
        if n_clusters > 1:
            if clustering_enabled:
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
                    c_poly = __gdf2poly(c_gdf)
                    clusters.append(c_poly)
            else:
                raise sliderule.FatalError("Clustering support not enabled; unable to import sklearn package")

    # update timing profiles
    profiles[toregion.__name__] = time.perf_counter() - tstart

    # return region
    return {
        "gdf": gdf,
        "poly": polygon, # convex hull of polygons
        "clusters": clusters, # list of polygon clusters for cmr request
        "raster": {
            "data": datafile, # geojson file
            "length": len(datafile), # geojson file length
            "cellsize": cellsize  # untis are in crs/projection
        }
    }

#
# Get Version
#
def get_version ():
    '''
    Get the version information for the running servers and Python client

    Returns
    -------
    dict
        dictionary of version information
    '''
    return sliderule.get_version()
