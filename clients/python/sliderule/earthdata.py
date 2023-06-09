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
import copy
import json
import ssl
import urllib.request
import requests
import logging
import numpy
import geopandas
from datetime import datetime
from shapely.geometry.multipolygon import MultiPolygon
from shapely.geometry import Polygon
import sliderule


###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300
max_requested_resources = DEFAULT_MAX_REQUESTED_RESOURCES

# best effort match of datasets to providers and versions for earthdata
DATASETS = {
    "ATL03":                                               {"provider": "NSIDC_ECS",   "version": "005",  "api": "cmr",   "formats": [".h5"],    "collections": [] },
    "ATL06":                                               {"provider": "NSIDC_ECS",   "version": "005",  "api": "cmr",   "formats": [".h5"],    "collections": []},
    "ATL08":                                               {"provider": "NSIDC_ECS",   "version": "005",  "api": "cmr",   "formats": [".h5"],    "collections": []},
    "GEDI01_B":                                            {"provider": "LPDAAC_ECS",  "version": "002",  "api": "cmr",   "formats": [".h5"],    "collections": []},
    "GEDI02_A":                                            {"provider": "LPDAAC_ECS",  "version": "002",  "api": "cmr",   "formats": [".h5"],    "collections": []},
    "GEDI02_B":                                            {"provider": "LPDAAC_ECS",  "version": "002",  "api": "cmr",   "formats": [".tiff"],  "collections": []},
    "GEDI_L3_LandSurface_Metrics_V2_1952":                 {"provider": "ORNL_CLOUD",  "version": None,   "api": "cmr",   "formats": [".h5"],    "collections": []},
    "GEDI_L4A_AGB_Density_V2_1_2056":                      {"provider": "ORNL_CLOUD",  "version": None,   "api": "cmr",   "formats": [".h5"],    "collections": []},
    "GEDI_L4B_Gridded_Biomass_2017":                       {"provider": "ORNL_CLOUD",  "version": None,   "api": "cmr",   "formats": [".tiff"],  "collections": []},
    "HLS":                                                 {"provider": "LPCLOUD",     "version": None,   "api": "stac",  "formats": [".tiff"],  "collections": ["HLSS30.v2.0", "HLSL30.v2.0"]},
    "Digital Elevation Model (DEM) 1 meter":               {"provider": "USGS",        "version": None,   "api": "tnm",   "formats": [".tiff"],  "collections": []},
    "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1":  {"provider": "POCLOUD",     "version": None,   "api": "cmr",   "formats": [".nc"],    "collections": ["C2147947806-POCLOUD"]},
    "SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1":        {"provider": "POCLOUD",     "version": None,   "api": "cmr",   "formats": [".nc"],    "collections": ["C2152046451-POCLOUD"]}
}

# upper limit on resources returned from CMR query per request
CMR_PAGE_SIZE = 2000

# upper limit on resources returned from STAC query per request
STAC_PAGE_SIZE = 100

# upper limit on resources returned from TNM query per request
TNM_PAGE_SIZE = 100

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

def __cmr_filter_urls(search_results, data_formats):
    """Select only the desired data files from CMR response."""
    if 'feed' not in search_results or 'entry' not in search_results['feed']:
        return []
    entries = [e['links']
               for e in search_results['feed']['entry']
               if 'links' in e]
    # Flatten "entries" to a simple list of links
    links = list(itertools.chain(*entries))
    # Build unique filenames
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
        if any([extension in link['href'] for extension in data_formats]):
            resource = link['href'].split("/")[-1]
            urls.append(resource)
    # return filtered urls
    return urls

def __cmr_granule_metadata(search_results):
    """Get the metadata for CMR returned granules"""
    # GeoDataFrame with granule metadata
    granule_metadata = sliderule.emptyframe()
    # return empty dataframe if no CMR entries
    if 'feed' not in search_results or 'entry' not in search_results['feed']:
        return granule_metadata
    # for each CMR entry
    for e in search_results['feed']['entry']:
        # columns for dataframe
        columns = {}
        # granule title and identifiers
        columns['title'] = e['title']
        columns['collection_concept_id'] = e['collection_concept_id']
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
            polygons = []
            # for each polygon
            for poly in e['polygons'][0]:
                coords = [float(i) for i in poly.split()]
                polygons.append(Polygon(zip(coords[1::2], coords[::2])))
            # generate multipolygon from list of polygons
            geometry = MultiPolygon(polygons)
        else:
            geometry, = geopandas.points_from_xy([None], [None])
        # Build GeoDataFrame (default geometry is crs=EPSG_MERCATOR)
        gdf = geopandas.GeoDataFrame(df, geometry=[geometry], crs=sliderule.EPSG_MERCATOR)
        # append to combined GeoDataFrame and catch warnings
        granule_metadata = geopandas.pd.concat([granule_metadata, gdf])
    # return granule metadata
    # - time start and time end
    # - time granule was updated
    # - granule size in bits
    # - polygons as geodataframe geometry
    return granule_metadata

def __cmr_query(provider, short_name, version, time_start, time_end, **kwargs):
    """Perform a scrolling CMR query for files matching input criteria."""
    kwargs.setdefault('polygon',None)
    kwargs.setdefault('name_filter',None)
    kwargs.setdefault('return_metadata',False)
    # build params
    params = '&short_name={0}'.format(short_name)
    if version != None:
        params += '&version={0}'.format(version)
    if time_start != None and time_end != None:
        params += '&temporal[]={0},{1}'.format(time_start, time_end)
    if kwargs['polygon']:
        params += '&polygon={0}'.format(kwargs['polygon'])
    if kwargs['name_filter']:
        params += '&options[producer_granule_id][pattern]=true'
        params += '&producer_granule_id[]=' + kwargs['name_filter']
    CMR_URL = 'https://cmr.earthdata.nasa.gov'
    cmr_query_url = ('{0}/search/granules.json?provider={1}'
                     '&sort_key[]=start_date&sort_key[]=producer_granule_id'
                     '&scroll=true&page_size={2}'.format(CMR_URL, provider, CMR_PAGE_SIZE))
    cmr_query_url += params
    logger.debug('cmr request={0}\n'.format(cmr_query_url))

    cmr_scroll_id = None
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    urls = []
    # GeoDataFrame with granule metadata
    metadata = sliderule.emptyframe()
    while True:
        req = urllib.request.Request(cmr_query_url)
        if cmr_scroll_id:
            req.add_header('cmr-scroll-id', cmr_scroll_id)
        response = urllib.request.urlopen(req, context=ctx)
        if not cmr_scroll_id:
            # Python 2 and 3 have different case for the http headers
            headers = {k.lower(): v for k, v in dict(response.info()).items()}
            cmr_scroll_id = headers['cmr-scroll-id']
        search_page = response.read()
        search_page = json.loads(search_page.decode('utf-8'))
        url_scroll_results = __cmr_filter_urls(search_page, DATASETS[short_name]["formats"])
        if not url_scroll_results:
            break
        urls += url_scroll_results
        # query for granule metadata and polygons
        if kwargs['return_metadata']:
            metadata_results = __cmr_granule_metadata(search_page)
        else:
            metadata_results = geopandas.pd.DataFrame([None for _ in url_scroll_results])
        # append granule metadata
        metadata = geopandas.pd.concat([metadata, metadata_results])

    return (urls,metadata)

###############################################################################
# CMR UTILITIES
###############################################################################

#
# Perform a CMR Search Request
#
def __cmr_search(provider, short_name, version, polygons, time_start, time_end, return_metadata, name_filter):

    global max_requested_resources

    # initialize return value
    resources = {} # [<url>] = <polygon>

    # iterate through each polygon (or none if none supplied)
    for polygon in polygons:
        urls = []
        metadata = sliderule.emptyframe()

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
                urls,metadata = __cmr_query(provider, short_name, version, time_start, time_end, polygon=polystr, return_metadata=return_metadata, name_filter=name_filter)
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

    # Check Resources are Under Limit
    if(len(url_list) > max_requested_resources):
        raise sliderule.FatalError('Exceeded maximum requested resources: {} (current max is {})\nConsider using earthdata.set_max_resources to set a higher limit.'.format(len(url_list), max_requested_resources))
    else:
        logger.info("Identified %d resources to process", len(url_list))

    if return_metadata:
        return (url_list,meta_list)
    else:
        return url_list

#
# Build a GeoJSON Response from STAC Query Response
#
def __build_geojson(rsps):
    geojson = rsps.json()
    del geojson["links"]
    if 'numberMatched' in geojson:
        del geojson['numberMatched']
    if 'numberReturned' in geojson:
        del geojson['numberReturned']
    for i in reversed(range(len(geojson["features"]))):
        del geojson["features"][i]["links"]
        del geojson["features"][i]["stac_version"]
        del geojson["features"][i]["stac_extensions"]
        del geojson["features"][i]["collection"]
        del geojson["features"][i]["bbox"]
        del geojson["features"][i]["assets"]["browse"]
        del geojson["features"][i]["assets"]["metadata"]
        propertiesDict = geojson["features"][i]["properties"]
        assetsDict = geojson["features"][i]["assets"]
        for val in assetsDict:
            if "href" in assetsDict[val]:
                propertiesDict[val] = assetsDict[val]["href"]
        del geojson["features"][i]["assets"]
    return geojson

#
# Perform a STAC Query
#
#   See https://cmr.earthdata.nasa.gov/stac/docs/index.html for details on API
#
def __stac_search(provider, short_name, collections, polygons, time_start, time_end):
    global max_requested_resources

    # attempt to fill in collections
    if collections == None:
        if short_name in DATASETS:
            collections = DATASETS[short_name]["collections"]
        else:
            raise sliderule.FatalError("Unable to determine collections for CMR query")

    # create requests context
    context = requests.Session()
    context.trust_env = False # prevent requests from attaching credentials from environment

    # build stac request
    url = 'https://cmr.earthdata.nasa.gov/stac/{}/search'.format(provider)
    headers = {'Content-Type': 'application/json'}
    rqst = {
        "limit": STAC_PAGE_SIZE,
        "datetime": '{}/{}'.format(time_start, time_end),
        "collections": collections,
    }

    # add polygon if provided
    if polygons:
        rqst["intersects"] = {
            "type": "Polygon",
            "coordinates": [[[coord["lon"], coord["lat"]] for coord in polygon] for polygon in polygons]
        }

    # make initial stac request
    data = context.post(url, data=json.dumps(rqst), headers=headers)
    data.raise_for_status()
    geojson = __build_geojson(data)

    # iterate through additional pages if not all returned
    num_returned = geojson["context"]["returned"]
    num_matched = geojson["context"]["matched"]
    if num_matched > max_requested_resources:
        logger.warn("Number of matched resources truncated from {} to {}".format(num_matched, max_requested_resources))
        num_matched = max_requested_resources
    num_pages = int((num_matched  + (num_returned - 1)) / num_returned)
    for page in range(2, num_pages+1):
        rqst["page"] = page
        data = context.post(url, data=json.dumps(rqst), headers=headers)
        data.raise_for_status()
        _geojson = __build_geojson(data)
        geojson["features"] += _geojson["features"]
    geojson["context"]["returned"] = num_matched
    geojson["context"]["limit"] = max_requested_resources

    # return geojson dictionary
    return geojson

#
# Get Provider from DATASETS
#
def __get_provider(short_name):

    # check parameters
    if short_name == None:
        raise sliderule.FatalError("Must supply short name to CMR query")
    elif short_name not in DATASETS:
        raise sliderule.FatalError("Must supply a supported dataset: " + short_name)

    # attempt to fill in provider
    provider = DATASETS[short_name]["provider"]
    if provider == None:
        raise sliderule.FatalError("Unable to determine provider for CMR query")

    # return provider string (cannot be None)
    return provider

#
# Format Polygons for Request
#
def __format_polygons(polygon):

    polygons = [None]

    # create list of polygons
    if polygon and len(polygon) > 0:
        if type(polygon[0]) == dict:
            polygons = [copy.deepcopy(polygon)]
        elif type(polygon[0] == list):
            polygons = copy.deepcopy(polygon)

    # return list of polygons
    return polygons

###############################################################################
# APIs
###############################################################################

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
def cmr(short_name=None, version=None, polygon=None, time_start='2018-01-01T00:00:00Z', time_end=datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"), return_metadata=False, name_filter=None):
    '''
    Query the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov>`_ for a list of data within temporal and spatial parameters

    Parameters
    ----------
        short_name:         str
                            dataset short name as defined in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
        version:            str
                            dataset version string, leave as None to get latest support version
        polygon:            list
                            either a single list of longitude,latitude in counter-clockwise order with first and last point matching, defining region of interest (see `polygons </web/rtd/user_guide/SlideRule.html#polygons>`_), or a list of such lists when the region includes more than one polygon
        time_start:         str
                            starting time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        time_end:           str
                            ending time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        return_metadata:    bool
                            flag indicating whether metadata associated with the query is returned back to the user
        name_filter:        str
                            filter to apply to resources returned by query

    Returns
    -------
    list
        files (granules) for the dataset fitting the spatial and temporal parameters

    Examples
    --------
        >>> from sliderule import earthdata
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033},
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> granules = earthdata.cmr(short_name='ATL06', polygon=region)
        >>> granules
        ['ATL03_20181017222812_02950102_003_01.h5', 'ATL03_20181110092841_06530106_003_01.h5', ... 'ATL03_20201111102237_07370902_003_01.h5']
    '''

    # get provider
    provider = __get_provider(short_name)

    # attempt to fill in version
    if version == None:
        version = DATASETS[short_name]["version"]

    # create list of polygons
    polygons = __format_polygons(polygon)

    # perform query
    return __cmr_search(provider, short_name, version, polygons, time_start, time_end, return_metadata, name_filter)

#
#  SpatioTemporal Asset Catalog
#
def stac(short_name=None, collections=None, polygon=None, time_start='2018-01-01T00:00:00Z', time_end=datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"), as_str=True):
    '''
    Perform a STAC query of the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov>`_ catalog for a list of data within temporal and spatial parameters

    Parameters
    ----------
        short_name:     str
                        dataset short name as defined in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
        collections:    list
                        list of dataset collections as specified by CMR, leave as None to use defaults
        polygon:        list
                        either a single list of longitude,latitude in counter-clockwise order with first and last point matching, defining region of interest (see `polygons </web/rtd/user_guide/SlideRule.html#polygons>`_), or a list of such lists when the region includes more than one polygon
        time_start:     str
                        starting time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        time_end:       str
                        ending time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
        as_str:         bool
                        whether to return geojson as a dictionary or string

    Returns
    -------
    str
        geojson of the feature set returned by the query
        {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "<id>",
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [..]
                    },
                    "properties": {
                        "datetime": "YYYY-MM-DDTHH:MM:SS.SSSZ",
                        "start_datetime": "YYYY-MM-DDTHH:MM:SS.SSSZ",
                        "end_datetime": "YYYY-MM-DDTHH:MM:SS.SSSZ",
                        "<tag>": "<url>",
                        ..
                    }
                },
                ..
            ],
            "stac_version": "<version>"
            "context": {
                "returned": <returned>,
                "limit": <limit>,
                "matched": <matched>
            }
        }

    Examples
    --------
        >>> from sliderule import earthdata
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033},
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> geojson = earthdata.stac(short_name='HLS', polygon=region)
    '''
    # get provider
    provider = __get_provider(short_name)

    # check collections
    if collections == None:
        collections = DATASETS[short_name]["collections"]

    # create list of polygons
    polygons = None
    if polygon:
        polygons = __format_polygons(polygon)

    # perform query
    geojson = __stac_search(provider, short_name, collections, polygons, time_start, time_end)

    # return
    if as_str:
        return json.dumps(geojson)
    else:
        return geojson

#
#  The National Map
#
def tnm(short_name, polygon, time_start=None, time_end=datetime.utcnow().strftime("%Y-%m-%d"), as_str=True):
    '''
    Query `USGS National Map API <https://tnmaccess.nationalmap.gov/api/v1/products>`_ for a list of data within temporal and spatial parameters.  See https://apps.nationalmap.gov/help/documents/TNMAccessAPIDocumentation/TNMAccessAPIDocumentation.pdf for more details on the API.

    Parameters
    ----------
        short_name:         str
                            dataset name
        polygon:            list
                            a single list of longitude,latitude in counter-clockwise order with first and last point matching, defining region of interest (see `polygons </web/rtd/user_guide/SlideRule.html#polygons>`_)
        time_start:         str
                            starting time for query in format ``<year>-<month>-<day>``
        time_end:           str
                            ending time for query in format ``<year>-<month>-<day>``

    Returns
    -------
    dict
        geojson of resources intersecting area of interest

    Examples
    --------
        >>> from sliderule import earthdata
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033},
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        >>> geojson
        {'type': 'FeatureCollection', 'features': [{'type': 'Feature', 'id': '5eaa4a0582cefae35a21ee8c', 'geometry': {'type': 'Polygon'...
    '''
    # Flatten polygon (the list must be formated as 'x y, x y, x y, x y, x y', the documentation is incorrect)
    coord_list = []
    for coord in polygon:
        coord_list.append('{} {}'.format(coord["lon"], coord["lat"]))
    poly_str = ', '.join(coord_list)

    # Create requests context
    context = requests.Session()
    context.trust_env = False # prevent requests from attaching credentials from environment

    # Make paged requests to collect all resources that intersect area of interest
    items = []
    while True:

        # Build request
        url='https://tnmaccess.nationalmap.gov/api/v1/products'
        rqst = {
            "datasets":     short_name,
            "polygon":      poly_str,
            "prodFormats":  'GeoTIFF',
            "outputFormat": 'JSON',
            "max":          TNM_PAGE_SIZE,
            "offset":       len(items)
        }

        # Optional request parameters
        if time_start != None:
            rqst["start"] = time_start
            rqst["stop"] = time_end

        # Make request
        rsps = context.get(url, params=rqst)
        rsps.raise_for_status()
        data = json.loads(rsps.content)
        items += data['items']
        if len(items) == data['total']:
            break

    # Create GeoJSON
    geojson = {
        "type": "FeatureCollection",
        "features": []
    }
    for i in range(len(items)):
        geojson['features'].append({"type": "Feature"})
        id = items[i]['sourceId']
        geojson['features'][i].update({"id": id})

        # Create polygon from bounding box
        minX = items[i]['boundingBox']['minX']
        maxX = items[i]['boundingBox']['maxX']
        minY = items[i]['boundingBox']['minY']
        maxY = items[i]['boundingBox']['maxY']

        # Counter-clockwise for interior of bounding box
        coordList = [[[minX, minY], [maxX, minY], [maxX, maxY], [minX, maxY], [minX, minY]]]

        geometryDic = {"type": "Polygon", "coordinates": coordList}
        geojson['features'][i].update({"geometry": geometryDic})

        date = items[i]['lastUpdated']
        pydate = datetime.fromisoformat(date)
        date = pydate.astimezone(None).isoformat()
        url  = items[i]['urls']['TIFF']
        propertiesDict = {"datetime": date, "url": url }
        geojson['features'][i].update({"properties": propertiesDict})

    # Return GeoJSON
    if as_str:
        return json.dumps(geojson)
    else:
        return geojson

