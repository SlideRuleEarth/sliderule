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

import json
from datetime import datetime
from sliderule import sliderule

#
# Maximum Requested Resources Allowed
#
MaxRequestedResources = None

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
        >>> from sliderule import earthdata
        >>> earthdata.set_max_resources(1000)
    '''
    global MaxRequestedResources
    MaxRequestedResources = max_resources

#
#  Asset Metadata Service
#
def ams(short_name=None, version=None, polygon=None, time_start=None, time_end=None, return_metadata=False, name_filter=None):
    '''
    Query the SlideRule Asset Metadata Service (AMS) for a list of data within temporal and spatial parameters

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
    '''
    global MaxRequestedResources

    # Build Parameters
    parms = {
        k: v for k, v in {
        "api": "ams",
        "short_name": short_name,
        "poly": polygon,
        "t0": time_start,
        "t1": time_end,
        "with_meta": return_metadata,
        "name_filter": name_filter,
        "max_resources": MaxRequestedResources
    }.items() if v is not None}

    # Make Request
    return sliderule.source("earthdata", parms, rethrow=True)

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
    global MaxRequestedResources

    # Build Parameters
    parms = {
        k: v for k, v in {
        "api": "cmr",
        "cmr": {},
        "short_name": short_name,
        "poly": polygon,
        "t0": time_start,
        "t1": time_end,
        "with_meta": return_metadata,
        "name_filter": name_filter,
        "max_resources": MaxRequestedResources
    }.items() if v is not None}

    # Set Version (special case)
    if version != None:
        parms["cmr"]["version"] = version

    # Make Request
    return sliderule.source("earthdata", parms, rethrow=True)

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
    global MaxRequestedResources

    # Build Parameters
    parms = {
        k: v for k, v in {
        "api": "stac",
        "short_name": short_name,
        "collections": collections,
        "poly": polygon,
        "t0": time_start,
        "t1": time_end,
        "max_resources": MaxRequestedResources
    }.items() if v is not None}

    # Make Request
    geojson = sliderule.source("earthdata", parms, rethrow=True)

    # Return GeoJSON
    if as_str:
        return json.dumps(geojson)
    else:
        return geojson

#
#  The National Map
#
def tnm(short_name, polygon=None, time_start=None, time_end=datetime.utcnow().strftime("%Y-%m-%d"), as_str=True):
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
    global MaxRequestedResources

    # Build Parameters
    parms = {
        k: v for k, v in {
        "api": "tnm",
        "short_name": short_name,
        "poly": polygon,
        "t0": time_start,
        "t1": time_end,
        "max_resources": MaxRequestedResources
    }.items() if v is not None}

    # Make Request
    geojson = sliderule.source("earthdata", parms, rethrow=True)

    # Return GeoJSON
    if as_str:
        return json.dumps(geojson)
    else:
        return geojson

#
#  Search
#
def search(parm, resources=None):
    '''
    This is the highest-level API call and attempts to automatically determine which service needs to be queried to return the resouces being requested.

    Parameters
    ----------
        parm:               dict
                            request parameters

    Returns
    -------
    list
        list of resources to process

    Notes
    -----
    The ``asset`` parameter must be supplied

    Examples
    --------
        >>> from sliderule import earthdata
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033},
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> parms = {"asset": "icesat2", "poly": region, "cycle": 20, "rgt": 232}
        >>> resources = earthdata.search(parms)
    '''
    global MaxRequestedResources
    return sliderule.source("earthdata", parm, rethrow=True)
