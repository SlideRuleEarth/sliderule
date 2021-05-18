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
 
import itertools
import json
import ssl
import urllib.request
import datetime
import logging
import concurrent.futures
import warnings
import numpy
import geopandas
from shapely.geometry.multipolygon import MultiPolygon
import sliderule

###############################################################################
# GLOBALS
###############################################################################

# configuration
SERVER_SCALE_FACTOR = 6

# create logger
logger = logging.getLogger(__name__)

# output dictionary keys
keys = ['segment_id','spot','delta_time','lat','lon','h_mean','dh_fit_dx','dh_fit_dy','rgt','cycle']

# output variable data types
dtypes = ['i','u1','f','f','f','f','f','f','f','u2','u2']

# default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300
max_requested_resources = DEFAULT_MAX_REQUESTED_RESOURCES

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

def __cmr_search(short_name, version, time_start, time_end, polygon=None):
    """Perform a scrolling CMR query for files matching input criteria."""
    params = '&short_name={0}'.format(short_name)
    params += __build_version_query_params(version)
    params += '&temporal[]={0},{1}'.format(time_start, time_end)
    if polygon:
        params += '&polygon={0}'.format(polygon)
    cmr_query_url = CMR_FILE_URL + params
    logger.debug('cmr request={0}\n'.format(cmr_query_url))

    cmr_scroll_id = None
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    urls = []
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

    return urls

###############################################################################
# SLIDERULE UTILITIES
###############################################################################

#
#  __flatten_atl06
#
def __flatten_atl06(rsps):
    """
    rsps: array of responses from streaming source call to atl06 endpoint
    """
    global keys, dtypes
    # total length of flattened response
    flatten = numpy.sum([len(r['elevation']) for i,r in enumerate(rsps)]).astype(numpy.int)
    # python dictionary with flattened variables
    flattened = {}
    for key,dtype in zip(keys,dtypes):
        flattened[key] = numpy.zeros((flatten),dtype=dtype)

    # counter variable for flattening responses
    c = 0
    # flatten response
    for i,r in enumerate(rsps):
        for j,v in enumerate(r['elevation']):
            # add each variable
            for key,dtype in zip(keys,dtypes):
                flattened[key][c] = numpy.array(v[key],dtype=dtype)
            # add to counter
            c += 1

    return flattened

#
#  __get_values
#
def __get_values(data, dtype, size):
    """
    data:   tuple of bytes
    dtype:  element of codedtype
    size:   bytes in data
    """

    codedtype2nptype = {
        0:  numpy.int8,     # INT8  
        1:  numpy.int16,    # INT16
        2:  numpy.int32,    # INT32
        3:  numpy.int64,    # INT64
        4:  numpy.uint8,    # UINT8
        5:  numpy.uint16,   # UINT16
        6:  numpy.uint32,   # UINT32
        7:  numpy.uint64,   # UINT64
        8:  numpy.byte,     # BITFIELD
        9:  numpy.single,   # FLOAT  
        10: numpy.double,   # DOUBLE  
        11: numpy.byte,     # TIME8  
        12: numpy.byte      # STRING 
    }
  
    raw = bytes(data)
    datatype = codedtype2nptype[dtype]
    num_elements = int(size / numpy.dtype(datatype).itemsize)
    slicesize = num_elements * numpy.dtype(datatype).itemsize # truncates partial bytes
    values = numpy.frombuffer(raw[:slicesize], dtype=datatype, count=num_elements)

    return values

#
#  __query_resources
#
def __query_resources(parm):

    # Check Parameters are Valid
    if ("poly" not in parm) and ("t0" not in parm) and ("t1" not in parm):
        logger.error("Must supply some bounding parameters with request (poly, t0, t1)")
        return []

    # Pull Out Polygon #
    polygon = None
    if "poly" in parm:
        polygon = parm["poly"]

    # Pull Out Time Period #
    time_start = None
    time_end = None
    if "t0" in parm:
        time_start = parm["t0"]
    if "t1" in parm:
        time_start = parm["t1"]

    # Make CMR Request #
    resources = cmr(polygon, time_start, time_end)
    if(len(resources) > max_requested_resources):
        logger.info("Exceeded maximum requested resources: %d (current max is %d)", len(resources), max_requested_resources)
        logger.info("Consider using icesat2.set_max_resources to set a higher limit")
        resources = []
    else:
        logger.info("Identified %d resources to process", len(resources))

    # Return Resources #
    return resources

#
#  __query_servers
#
def __query_servers(max_workers):

    # Update Available Servers #
    num_servers = sliderule.update_available_servers()
    if max_workers <= 0:
        max_workers = num_servers * SERVER_SCALE_FACTOR

    # Check if Servers are Available #
    if max_workers <= 0:
        logger.error("There are no servers available to fulfill this request")
        return
    else:
        logger.info("Allocating %d workers across %d processing nodes", max_workers, num_servers)
    
    # Return Number of Workers #
    return max_workers

#
#  __parallelize
#
def __parallelize(function, parm, resources, max_workers, block, *args):

    # For Blocking Calls
    if block:

        # Make Parallel Processing Requests
        results = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = [executor.submit(function, parm, resource, *args) for resource in resources]

            # Wait for Results
            result_cnt = 0
            for future in concurrent.futures.as_completed(futures):
                result_cnt += 1
                logger.info("Results returned for %d out of %d resources", result_cnt, len(resources))
                if len(results) == 0:
                    results = future.result()
                else:
                    result = future.result()
                    for element in result:
                        if element not in results:
                            logger.error("Unable to construct results with element: %s", element)
                            continue
                        results[element] += result[element]

        # Return Results
        return results

    # For Non-Blocking Calls
    else:

        # Create Thread Pool
        executor = concurrent.futures.ThreadPoolExecutor(max_workers=max_workers)

        # Return List of Futures for Parallel Processing Request
        return [executor.submit(function, parm, resource, *args) for resource in resources]

###############################################################################
# APIs
###############################################################################

#
#  INIT
#
def init (url, verbose=False, max_resources=DEFAULT_MAX_REQUESTED_RESOURCES, max_errors=3):
    sliderule.set_url(url)
    sliderule.set_verbose(verbose)
    sliderule.set_max_errors(max_errors)
    set_max_resources(max_resources)

#
#  SET MAX RESOURCES
#
def set_max_resources (max_resources):
    global max_requested_resources
    max_requested_resources = max_resources

#
#  COMMON METADATA REPOSITORY
#
def cmr (polygon=None, time_start=None, time_end=None, version='003', short_name='ATL03'):
    """
    polygon: list of longitude,latitude in counter-clockwise order with first and last point matching;
             - e.g. [ {"lon": -115.43, "lat": 37.40},
                      {"lon": -109.55, "lat": 37.58},
                      {"lon": -109.38, "lat": 43.28},
                      {"lon": -115.29, "lat": 43.05},
                      {"lon": -115.43, "lat": 37.40} ]
    time_*: UTC time (i.e. "zulu" or "gmt");
            expressed in the following format: <year>-<month>-<day>T<hour>:<minute>:<second>Z
    """

    url_list = []

    # set default start time to start of ICESat-2 mission
    if not time_start:
        time_start = '2018-10-13T00:00:00Z'
    # set default stop time to current time
    if not time_end:
        now = datetime.datetime.utcnow()
        time_end = now.strftime("%Y-%m-%dT%H:%M:%SZ")

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
            url_list = __cmr_search(short_name, version, time_start, time_end, polystr)
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

    return url_list

#
#  ATL06
#
def atl06 (parm, resource, asset="atlas-s3", track=0, as_numpy=False):

    # Build ATL06 Request
    rqst = {
        "atl03-asset" : asset,
        "resource": resource,
        "track": track,
        "parms": parm
    }

    # Execute ATL06 Algorithm
    rsps = sliderule.source("atl06", rqst, stream=True)

    # Flatten Responses
    if as_numpy:
        rsps = __flatten_atl06(rsps)
    else:
        flattened = {}
        if (len(rsps) > 0) and (rsps[0]['__rectype'] == 'atl06rec' or rsps[0]['__rectype'] == 'atl06rec-compact'):
            for element in rsps[0]["elevation"][0].keys():
                flattened[element] = [rsps[r]["elevation"][i][element] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
        else: # Unrecognized
            logger.warning("unable to process resource %s: no elements", resource)
        rsps = flattened

    # Return Responses
    return rsps

#
#  PARALLEL ATL06
#
def atl06p(parm, asset="atlas-s3", track=0, as_numpy=False, max_workers=0, block=True):

    # Query System #
    resources = __query_resources(parm)
    max_workers = __query_servers(max_workers)
    return __parallelize(atl06, parm, resources, max_workers, block, asset, track, as_numpy)

#
#  Subsetted ATL03
#
def atl03s (parm, resource, asset="atlas-s3", track=0):

    # Build ATL06 Request
    rqst = {
        "atl03-asset" : asset,
        "resource": resource,
        "track": track,
        "parms": parm
    }

    # Execute ATL06 Algorithm
    rsps = sliderule.source("atl03s", rqst, stream=True)

    # Flatten Responses
    flattened = {}
    if (len(rsps) > 0) and (rsps[0]['__rectype'] == 'atl03rec'):
        for element in rsps[0].keys():
            if type(rsps[0][element]) == tuple:
                flattened[element] = [rsps[r][element][i] for r in range(len(rsps)) for i in range(2)]
            elif type(rsps[0][element]) == int:
                flattened[element] = [rsps[r][element] for r in range(len(rsps)) for i in range(2)]
    else: # Unrecognized
        logger.warning("unable to process resource %s: no elements", resource)

    # Return Responses
    return flattened

#
#  PARALLEL SUBSETTED ATL03
#
def atl03sp(parm, asset="atlas-s3", track=0, max_workers=0, block=True):

    resources = __query_resources(parm)
    max_workers = __query_servers(max_workers)
    return __parallelize(atl03s, parm, resources, max_workers, block, asset, track)

#
#  H5
#
def h5 (dataset, resource, asset="atlas-s3", datatype=sliderule.datatypes["DYNAMIC"], col=0, startrow=0, numrows=ALL_ROWS):

    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "dataset": dataset,
        "datatype": datatype,
        "col": col,
        "startrow": startrow,
        "numrows": numrows,
        "id": 0
    }

    # Read H5 File
    rsps = sliderule.source("h5", rqst, stream=True)

    # Build Record Data
    rsps_datatype = rsps[0]["datatype"]
    rsps_data = bytearray()
    rsps_size = 0
    for d in rsps:
        rsps_data += bytearray(d["data"])
        rsps_size += d["size"]

    # Get Values
    values = __get_values(rsps_data, rsps_datatype, rsps_size)

    # Return Response
    return values

#
#  H5P
#
def h5p (datasets, resource, asset="atlas-s3"):

    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "datasets": datasets,
    }

    # Read H5 File
    rsps = sliderule.source("h5p", rqst, stream=True)

    # Build Record Data
    results = {}
    for result in rsps:
        results[result["dataset"]] = __get_values(result["data"], result["datatype"], result["size"])

    # Return Results
    return results

#
# TO REGION
#
def toregion (filename, tolerance=0.0):

    # initialize regions #
    regions = []

    # native format #
    if filename.find(".json") > 1:
        with open(filename) as regionfile:
            region = json.load(regionfile)["region"]
            regions.append(region)

    # geojson or shapefile format #
    elif (filename.find(".geojson") > 1) or (filename.find(".shp") > 1):
        polygons = geopandas.read_file(filename)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            polygons = polygons.buffer(tolerance)
        polygons = polygons.simplify(tolerance)
        for polygon in polygons.geometry:
            region = []
            for coord in list(polygon.exterior.coords):
                point = {"lon": coord[0], "lat": coord[1]}
                region.append(point)
            if len(region) > 0 and len(region) <= MAX_COORDS_IN_POLYGON:       
                regions.append(region)
            else:
                logger.warning("dropping polygon with unsupported length: %d (max is %d)", len(region), MAX_COORDS_IN_POLYGON)
    
    # determine winding of polygons #
    for r in range(len(regions)):
        region = regions[r]
        #              (x2          -        x1)        *        (y2          +        y1)
        wind = sum([(region[i+1]["lon"] - region[i]["lon"]) * (region[i+1]["lat"] + region[i]["lat"]) for i in range(len(region) - 1)])
        if wind > 0:
            # reverse direction (make counter-clockwise) #
            ccw_region = []
            for i in range(len(region), 0, -1):
                ccw_region.append(region[i - 1])
            # replace region with counter-clockwise version #
            regions[r] = ccw_region               

    # return region #
    return regions
