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

import time
import datetime
import logging
import warnings
import numpy
import geopandas
import sliderule
import cmr

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# profiling times for each major function
profiles = {}

# default asset
DEFAULT_ASSET="ornldaac-s3"

# default GEDI standard data product version
DEFAULT_GEDI_SDP_VERSION = '002'

# gps-based epoch for delta times
GEDI_SDP_EPOCH = datetime.datetime(2018, 1, 1)


###############################################################################
# LOCAL FUNCTIONS
###############################################################################

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
        resources,metadata = cmr.cmr(**kwargs)
    else:
        resources = cmr.cmr(**kwargs)

    # Check Resources are Under Limit
    if(len(resources) > cmr.max_requested_resources):
        raise sliderule.FatalError('Exceeded maximum requested granules: {} (current max is {})\nConsider using icesat2.set_max_resources to set a higher limit.'.format(len(resources), max_requested_resources))
    else:
        logger.info("Identified %d resources to process", len(resources))

    # Update Profile
    profiles[__query_resources.__name__] = time.perf_counter() - tstart

    # Return Resources
    if kwargs['return_metadata']:
        return (resources,metadata)
    else:
        return resources


###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=sliderule.service_url, verbose=False, max_resources=cmr.DEFAULT_MAX_REQUESTED_RESOURCES, loglevel=logging.CRITICAL, organization=sliderule.service_org, desired_nodes=None, time_to_live=60):
    '''
    Initializes the Python client for use with SlideRule and should be called before other GEDI API calls.
    This function is a wrapper for the `sliderule.init(...) function </rtds/api_reference/sliderule.html#init>`_.

    Parameters
    ----------
        max_resources:  int
                        maximum number of H5 granules to process in the request

    Examples
    --------
        >>> from sliderule import gedi
        >>> gedi.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, plugins=['gedi'])
    cmr.set_max_resources(max_resources) # set maximum number of resources allowed per request

#
#  Common Metadata Repository
#
def cmr(version=DEFAULT_GEDI_SDP_VERSION, short_name='GEDI02_B', **kwargs):
    '''
    Query the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov/search>`_ for a list of data within temporal and spatial parameters.
    Wrapper for the `cmr.cmr(...) function </rtd/api_reference/cmr.html#cmr>`_.
    '''
    return cmr.cmr(polygon=kwargs['polygon'], time_start=kwargs['time_start'], time_end=kwargs['time_end'], version=version, short_name=short_name)

#
#  GEDI L4A
#
def gedi04a (parm, resource, asset=DEFAULT_ASSET):
    '''
    Performs GEDI L4A subsetting returns gridded elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure subsetting process
    resource:   str
                GEDI HDF5 filename
    asset:      str
                data source asset

    Returns
    -------
    GeoDataFrame
        gridded footrpints
    '''
    return gedi04ap(parm, asset=asset, resources=[resource])

#
#  Parallel ATL06
#
def gedi04ap(parm, asset=DEFAULT_ASSET, version=DEFAULT_GEDI_SDP_VERSION, callbacks={}, resources=None, keep_id=False):
    '''
    Performs subsetting in parallel on GEDI data and returns gridded footprints.  This function expects that the **parm** argument
    includes a polygon which is used to fetch all available resources from the CMR system automatically.  If **resources** is specified
    then any polygon or resource filtering options supplied in **parm** are ignored.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure subsetting process
        asset:          str
                        data source asset
        version:        str
                        the version of the ATL03 data to use for processing
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        gridded footprints

    Examples
    --------
        >>> from sliderule import gedi
        >>> gedi.init()
        >>> region = [ {"lon":-105.82971551223244, "lat": 39.81983728534918},
        ...            {"lon":-105.30742121965137, "lat": 39.81983728534918},
        ...            {"lon":-105.30742121965137, "lat": 40.164048017973755},
        ...            {"lon":-105.82971551223244, "lat": 40.164048017973755},
        ...            {"lon":-105.82971551223244, "lat": 39.81983728534918} ]
        >>> parms = { "poly": region }
        >>> resources = ["GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5"]
        >>> asset = "ornldaac-s3"
        >>> rsps = gedi.gedi04ap(parms, asset=asset, resources=resources)
    '''
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not supplied)
        if resources == None:
            resources = __query_resources(parm, version)

        # Build ATL06 Request
        parm["asset"] = asset
        rqst = {
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source("gedi04ap", rqst, stream=True, callbacks=callbacks)

        # Flatten Responses
#        gdf = __flattenbatches(rsps, 'gedi04arec', 'footprint', parm, keep_id)
        gdf = sliderule.__emptyframe()

        # Return Response
        profiles[gedi04ap.__name__] = time.perf_counter() - tstart
        return gdf

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return sliderule.__emptyframe()
