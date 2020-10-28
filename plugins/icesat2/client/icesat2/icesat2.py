# python

import itertools
import json
import ssl
import urllib.request
import datetime
import numpy
import logging
import sliderule

###############################################################################
# GLOBALS
###############################################################################

# logging
logger = logging.getLogger(__name__)
# output dictionary keys
keys = ['segment_id','spot','delta_time','lat','lon','h_mean','dh_fit_dx','dh_fit_dy','rgt','cycle']
# output variable data types
dtypes = ['i','u1','f','f','f','f','f','f','f','u2','u2']

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
    logger.info('cmr request\t{0}\n'.format(cmr_query_url))

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
    rsps: array of responses from engine call to atl06 endpoint
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

###############################################################################
# APIs
###############################################################################

#
#  INIT
#
def init (url, verbose=False):
    sliderule.set_url(url)
    sliderule.set_verbose(verbose)

#
#  COMMON METADATA REPOSITORY
#
def cmr (polygon=None, time_start=None, time_end=None, version='003', short_name='ATL03'):
    """
    polygon: list of longitude,latitude in counter-clockwise order with first and last point matching;
             three formats are supported:
             1. string - e.g. '-115.43,37.40,-109.55,37.58,-109.38,43.28,-115.29,43.05,-115.43,37.40'
             1. list - e.g. [-115.43,37.40,-109.55,37.58,-109.38,43.28,-115.29,43.05,-115.43,37.40]
             1. dictionary - e.g. [ {"lon": -115.43, "lat": 37.40},
                                    {"lon": -109.55, "lat": 37.58},
                                    {"lon": -109.38, "lat": 43.28},
                                    {"lon": -115.29, "lat": 43.05},
                                    {"lon": -115.43, "lat": 37.40} ]
    time_*: UTC time (i.e. "zulu" or "gmt");
            expressed in the following format: <year>-<month>-<day>T<hour>:<minute>:<second>Z
    """
    # set default start time to start of ICESat-2 mission
    if not time_start:
        time_start = '2018-10-13T00:00:00Z'
    # set default stop time to current time
    if not time_end:
        now = datetime.datetime.utcnow()
        time_end = now.strftime("%Y-%m-%dT%H:%M:%SZ")
    # flatten polygon if structure list of lat/lon provided
    if polygon:
        if type(polygon) == list:
            # if polygon list of dictionaries ("lat", "lon"), then flatten
            if type(polygon[0]) == dict:
                flatpoly = []
                for p in polygon:
                    flatpoly.append(p["lon"])
                    flatpoly.append(p["lat"])
                polygon = flatpoly
            # convert list into string
            polygon = str(polygon)[1:-1]

        # remove all spaces as this will be embedded in a url
        polygon = polygon.replace(" ", "")



    ###### TEST CODE #####
    return ['ATL03_20181017222812_02950102_003_01.h5', 'ATL03_20181110092841_06530106_003_01.h5', 'ATL03_20181114092019_07140106_003_01.h5', 'ATL03_20181115210428_07370102_003_01.h5', 'ATL03_20181213075606_11560106_003_01.h5', 'ATL03_20181214194017_11790102_003_01.h5', 'ATL03_20190111063212_02110206_003_01.h5', 'ATL03_20190112181620_02340202_003_01.h5', 'ATL03_20190116180755_02950202_003_01.h5', 'ATL03_20190209050825_06530206_003_01.h5', 'ATL03_20190213050003_07140206_003_01.h5', 'ATL03_20190214164413_07370202_003_01.h5', 'ATL03_20190314033606_11560206_003_01.h5', 'ATL03_20190315152016_11790202_003_01.h5', 'ATL03_20190412021205_02110306_003_01.h5', 'ATL03_20190417134754_02950302_003_01.h5', 'ATL03_20190511004804_06530306_003_01.h5', 'ATL03_20190515003943_07140306_003_01.h5', 'ATL03_20190516122353_07370302_003_01.h5', 'ATL03_20190612231542_11560306_003_01.h5', 'ATL03_20190614105952_11790302_003_01.h5', 'ATL03_20190711215129_02110406_003_01.h5', 'ATL03_20190717092724_02950402_003_01.h5', 'ATL03_20190809202745_06530406_003_01.h5', 'ATL03_20190813201925_07140406_003_01.h5', 'ATL03_20190911185531_11560406_003_01.h5', 'ATL03_20190913063941_11790402_003_01.h5', 'ATL03_20191010173137_02110506_003_01.h5', 'ATL03_20191012051547_02340502_003_01.h5', 'ATL03_20191016050727_02950502_003_01.h5', 'ATL03_20191108160740_06530506_003_01.h5', 'ATL03_20191112155921_07140506_003_01.h5', 'ATL03_20191114034331_07370502_003_01.h5', 'ATL03_20191211143520_11560506_003_01.h5', 'ATL03_20200109131121_02110606_003_01.h5', 'ATL03_20200111005531_02340602_003_01.h5', 'ATL03_20200115004711_02950602_003_01.h5', 'ATL03_20200207114722_06530606_003_01.h5', 'ATL03_20200211113903_07140606_003_01.h5', 'ATL03_20200212232313_07370602_003_01.h5', 'ATL03_20200312215919_11790602_003_01.h5', 'ATL03_20200409085108_02110706_003_01.h5', 'ATL03_20200410203519_02340702_003_01.h5', 'ATL03_20200414202700_02950702_003_01.h5', 'ATL03_20200508072713_06530706_003_01.h5', 'ATL03_20200512071854_07140706_003_01.h5', 'ATL03_20200513190303_07370702_003_01.h5', 'ATL03_20200610055453_11560706_003_01.h5', 'ATL03_20200611173903_11790702_003_01.h5', 'ATL03_20200709043054_02110806_003_01.h5', 'ATL03_20200710161504_02340802_003_01.h5', 'ATL03_20200714160647_02950802_003_01.h5']




    # call into NSIDC routines to make CMR request
    try:
        url_list = __cmr_search(short_name, version, time_start, time_end, polygon)
    except urllib.error.HTTPError as e:
        url_list = []
        logger.error("HTTP Request Error:", e)
    except RuntimeError as e:
        url_list = []
        logger.error("Runtime Error:", e)

    return url_list

#
#  ATL06
#
def atl06 (parm, resource, asset="atl03-cloud", stages=["LSF"], track=0, as_numpy=True):
    # Build ATL06 Request
    rqst = {
        "atl03-asset" : asset,
        "resource": resource,
        "track": track,
        "stages": stages,
        "parms": parm
    }

    # Execute ATL06 Algorithm
    rsps = sliderule.engine("atl06", rqst)

    # Flatten Responses
    if as_numpy:
        rsps = __flatten_atl06(rsps)
    else:
        flattened = {}
        for element in rsps[0]["elevation"][0].keys():
            flattened[element] = [rsps[r]["elevation"][i][element] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
        rsps = flattened

    # Return Responses
    return rsps

#
#  GET VALUES
#
def get_values(data, dtype, size):
    """
    data:   tuple of bytes
    dtype:  element of datatypes
    size:   bytes in data
    """

    datatype2nptype = {
        sliderule.datatypes["TEXT"]:      numpy.byte,
        sliderule.datatypes["REAL"]:      numpy.double,
        sliderule.datatypes["INTEGER"]:   numpy.int32,
        sliderule.datatypes["DYNAMIC"]:   numpy.byte
    }

    raw = bytes(data)
    datatype = datatype2nptype[dtype]
    datasize = int(size / numpy.dtype(datatype).itemsize)
    slicesize = datasize * numpy.dtype(datatype).itemsize # truncates partial bytes
    values = numpy.frombuffer(raw[:slicesize], dtype=datatype, count=datasize)

    return values

#
#  H5
#
def h5 (dataset, resource, asset="atl03-cloud", datatype=sliderule.datatypes["REAL"]):
    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "dataset": dataset,
        "datatype": datatype,
        "id": 0
    }

    # Read H5 File
    rsps = sliderule.engine("h5", rqst)

    # Build Record Data
    datatype = rsps[0]["datatype"]
    data = ()
    size = 0
    for d in rsps:
        data = data + d["data"]
        size = size + d["size"]

    # Return Response Values
    return sliderule.get_values(data, datatype, size)
