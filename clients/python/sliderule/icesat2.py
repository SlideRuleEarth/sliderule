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

import logging
import numpy
import geopandas
import sliderule
from sliderule import earthdata, logger
from sliderule.session import Session, BASIC_TYPES, CODED_TYPE

###############################################################################
# GLOBALS
###############################################################################

# CRS used in ICESat-2 Standard Data Products
ICESAT2_CRS = "EPSG:7912"

# icesat2 parameters
CNF_POSSIBLE_TEP = -2
CNF_NOT_CONSIDERED = -1
CNF_BACKGROUND = 0
CNF_WITHIN_10M = 1
CNF_SURFACE_LOW = 2
CNF_SURFACE_MEDIUM = 3
CNF_SURFACE_HIGH = 4
SRT_DYNAMIC = -1
SRT_LAND = 0
SRT_OCEAN = 1
SRT_SEA_ICE = 2
SRT_LAND_ICE = 3
SRT_INLAND_WATER = 4
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
ATL08_WATER = 0
ATL08_LAND = 1
ATL08_SNOW = 2
ATL08_ICE = 3

# phoreal percentiles
P = { '5':   0, '10':  1, '15':  2, '20':  3, '25':  4, '30':  5, '35':  6, '40':  7, '45':  8, '50': 9,
      '55': 10, '60': 11, '65': 12, '70': 13, '75': 14, '80': 15, '85': 16, '90': 17, '95': 18 }

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=Session.PUBLIC_URL, verbose=False, max_resources=earthdata.DEFAULT_MAX_REQUESTED_RESOURCES, loglevel=logging.CRITICAL, organization=Session.PUBLIC_ORG, desired_nodes=None, time_to_live=60, bypass_dns=False, rethrow=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other ICESat-2 API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Parameters
    ----------
        max_resources:  int
                        maximum number of H5 granules to process in the request

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init()
    '''
    sliderule.init(url, verbose=verbose, loglevel=loglevel, organization=organization, desired_nodes=desired_nodes, time_to_live=time_to_live, bypass_dns=bypass_dns, rethrow=rethrow)
    earthdata.set_max_resources(max_resources) # set maximum number of resources allowed per request

#
#  ATL06
#
def atl06 (parm, resource):
    '''
    Performs ATL06-SR processing on ATL03 data and returns geolocated elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
    resource:   str
                ATL03 HDF5 filename

    Returns
    -------
    GeoDataFrame
        geolocated elevations (see `Elevations </web/rtd/user_guide/icesat2.html#elevations>`_)
    '''
    return atl06p(parm, resources=[resource])

#
#  Parallel ATL06
#
def atl06p(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
    '''
    Performs ATL06-SR processing in parallel on ATL03 data and returns geolocated elevations.  This function expects that the **parm** argument
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
                        parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

    Returns
    -------
    GeoDataFrame
        geolocated elevations (see `Elevations </web/rtd/user_guide/icesat2.html#elevations>`_)

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init("slideruleearth.io", True)
        >>> parms = { "cnf": 4, "ats": 20.0, "cnt": 10, "len": 40.0, "res": 20.0 }
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
    # Build Request
    rqst = __build_request(parm, resources)

    # Make API Processing Request
    rsps = sliderule.source("atl06p", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches(rsps, 'atl06rec', 'elevation', parm, keep_id, as_numpy_array, height_key)

#
#  Subsetted ATL06
#
def atl06s (parm, resource):
    '''
    Subsets ATL06 data given the polygon and time range provided and returns elevations

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        resource:   str
                    ATL06 HDF5 filename

    Returns
    -------
    GeoDataFrame
        ATL06 elevations
    '''
    return atl06sp(parm, resources=[resource])

#
#  Parallel Subsetted ATL06
#
def atl06sp(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
    '''
    Performs ATL06 subsetting in parallel on ATL06 data and returns elevation data.  Unlike the `atl06s <#atl06s>`_ function,
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
                        parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

    Returns
    -------
    GeoDataFrame
        ATL06 elevations
    '''
    # Build Request
    rqst = __build_request(parm, resources, default_asset="icesat2-atl06")

    # Make API Processing Request
    rsps = sliderule.source("atl06sp", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches(rsps, 'atl06srec', 'elevation', parm, keep_id, as_numpy_array, height_key)

#
#  Subsetted ATL13
#
def atl13s (parm, resource):
    '''
    Subsets ATL13 data given the polygon and time range provided and returns measurements

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL13 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        resource:   str
                    ATL13 HDF5 filename

    Returns
    -------
    GeoDataFrame
        ATL13 water measurements
    '''
    return atl13sp(parm, resources=[resource])

#
#  Parallel Subsetted ATL13
#
def atl13sp(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
    '''
    Performs ATL13 subsetting in parallel on ATL13 data and returns measurement data.  Unlike the `atl13s <#atl13s>`_ function,
    this function does not take a resource as a parameter; instead it is expected that the **parm** argument includes a polygon which
    is used to fetch all available resources from the CMR system automatically.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure ATL13 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL13_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

    Returns
    -------
    GeoDataFrame
        ATL13 water measurements
    '''
    # Build Request
    rqst = __build_request(parm, resources, default_asset="icesat2-atl13")

    # Make API Processing Request
    rsps = sliderule.source("atl13sp", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches(rsps, 'atl13srec', 'water', parm, keep_id, as_numpy_array, height_key)

#
#  Subsetted ATL03
#
def atl03s (parm, resource):
    '''
    Subsets ATL03 data given the polygon and time range provided and returns segments of photons

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        resource:   str
                    ATL03 HDF5 filename

    Returns
    -------
    GeoDataFrame
        ATL03 extents (see `Photon Segments </web/rtd/user_guide/icesat2.html#segmented-photon-data>`_)
    '''
    return atl03sp(parm, resources=[resource])

#
#  Parallel Subsetted ATL03
#
def atl03sp(parm, callbacks={}, resources=None, keep_id=False, height_key=None):
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
                        parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

    Returns
    -------
    GeoDataFrame
        ATL03 segments (see `Photon Segments </web/rtd/user_guide/icesat2.html#photon-segments>`_)
    '''
    rqst = __build_request(parm, resources)

    # Make Request
    rsps = sliderule.source("atl03sp", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches03(rsps, parm, keep_id, height_key)

#
#  Viewer ATL03
#
def atl03v (parm, resource):
    '''
    Subsets ATL03 data given the polygon and time range provided and returns counts of photons in segments

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        resource:   str
                    ATL03 HDF5 filename

    Returns
    -------
    GeoDataFrame
        ATL03 extents (see `Photon Segments </web/rtd/user_guide/icesat2.html#segmented-photon-data>`_)
    '''
    return atl03vp(parm, resources=[resource])

#
#  Parallel Viewer ATL03
#
def atl03vp(parm, callbacks={}, resources=None, keep_id=False):
    '''
    Performs ATL03 subsetting in parallel on ATL03 data and returns counts of photons in segments.  Unlike the `atl03v <#atl03v>`_ function,
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
                        parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        ATL03 segments (see `Photon Segments </web/rtd/user_guide/icesat2.html#photon-segments>`_)
    '''
    # Build Request
    rqst = __build_request(parm, resources)

    # Make Request
    rsps = sliderule.source("atl03vp", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches03v(rsps, parm, keep_id)

#
#  ATL08
#
def atl08 (parm, resource):
    '''
    Performs ATL08-PhoREAL processing on ATL03 and ATL08 data and returns geolocated elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
    resource:   str
                ATL03 HDF5 filename

    Returns
    -------
    GeoDataFrame
        geolocated vegatation statistics
    '''
    return atl08p(parm, resources=[resource])

#
#  Parallel ATL08
#
def atl08p(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
    '''
    Performs ATL08-PhoREAL processing in parallel on ATL03 and ATL08 data and returns geolocated vegatation statistics.  This function expects that the **parm** argument
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
                        parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/icesat2.html#parameters>`_)
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_005_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

    Returns
    -------
    GeoDataFrame
        geolocated vegetation statistics
    '''
    # Build Request
    rqst = __build_request(parm, resources)

    # Make Request
    rsps = sliderule.source("atl08p", rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches(rsps, 'atl08rec', 'vegetation', parm, keep_id, as_numpy_array, height_key)

#
#  ATL24 Photon Viewer
#
def atl24v(parm, resource):
    '''
    Provides statistics for the number of photons within a granule
    '''
    # Default the Asset
    if "asset" not in parm:
        parm["asset"] = "icesat2"

    # Build Request
    rqst = {
        "resource": resource,
        "parms": parm
    }

    # Make Request
    rsps = sliderule.source("atl24v", rqst, stream=False)

    # Return Response
    return rsps

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
# Calculate Laser Spot
#
def __calcspot(df):

    # Create dictionary mapping (sc_orient, track, pair) to spot
    map_spot = {(SC_BACKWARD,   1,  LEFT_PAIR):     1,
                (SC_BACKWARD,   1,  RIGHT_PAIR):    2,
                (SC_BACKWARD,   2,  LEFT_PAIR):     3,
                (SC_BACKWARD,   2,  RIGHT_PAIR):    4,
                (SC_BACKWARD,   3,  LEFT_PAIR):     5,
                (SC_BACKWARD,   3,  RIGHT_PAIR):    6,
                (SC_FORWARD,    1,  LEFT_PAIR):     6,
                (SC_FORWARD,    1,  RIGHT_PAIR):    5,
                (SC_FORWARD,    2,  LEFT_PAIR):     4,
                (SC_FORWARD,    2,  RIGHT_PAIR):    3,
                (SC_FORWARD,    3,  LEFT_PAIR):     2,
                (SC_FORWARD,    3,  RIGHT_PAIR):    1}
    # return spot column
    return geopandas.pd.Series(zip(df['sc_orient'], df['track'], df['pair'])).map(map_spot).values

#
# Get Ancillary Field Name
#
def __getancillaryfield(parm, field_rec):
    if field_rec['anc_type'] == 0:
        return parm['atl03_ph_fields'][field_rec['field_index']]
    if field_rec['anc_type'] == 1:
        return parm['atl03_geo_fields'][field_rec['field_index']]
    if field_rec['anc_type'] == 2:
        return parm['atl08_fields'][field_rec['field_index']]
    if field_rec['anc_type'] == 3:
        return parm['atl06_fields'][field_rec['field_index']]
    raise sliderule.FatalError(f'Invalid ancillary field type {field_rec["anc_type"]}')

#
# Flatten Batches
#
def __flattenbatches(rsps, rectype, batch_column, parm, keep_id, as_numpy_array, height_key):

    # Check for Responses
    if rsps == None:
        return sliderule.emptyframe(crs=ICESAT2_CRS)

    # Check for Output Options
    if "output" in parm:
        gdf = sliderule.procoutputfile(parm, rsps)
        return gdf

    # Flatten Records
    columns = {}
    records = []
    num_records = 0
    field_dictionary = {} # [<field_name>] = {"extent_id": [], <field_name>: []}
    file_dictionary = {} # [id] = "filename"
    if len(rsps) > 0:
        # Sort Records
        for rsp in rsps:
            if rectype in rsp['__rectype']:
                records += rsp,
                num_records += len(rsp[batch_column])
            elif 'ancfrec' == rsp['__rectype']:
                for field_rec in rsp['fields']:
                    extent_id = numpy.uint64(rsp['extent_id'])
                    field_name = __getancillaryfield(parm, field_rec)
                    if field_name not in field_dictionary:
                        field_dictionary[field_name] = {'extent_id': [], field_name: []}
                    field_dictionary[field_name]['extent_id'] += extent_id,
                    field_dictionary[field_name][field_name] += sliderule.getvalues(field_rec['value'], field_rec['datatype'], len(field_rec['value']), num_elements=1)[0],
            elif 'rsrec' == rsp['__rectype'] or 'zsrec' == rsp['__rectype'] or 'sdrec' == rsp['__rectype']:
                if rsp["num_samples"] <= 0:
                    continue
                # Get field names and set
                sample = rsp["samples"][0]
                field_names = list(sample.keys())
                field_names.remove("__rectype")
                field_set = rsp['key']
                if rsp["num_samples"] > 1:
                    as_numpy_array = True
                # On first time, build empty dictionary for field set associated with raster
                if field_set not in field_dictionary:
                    field_dictionary[field_set] = {'extent_id': []}
                    for field in field_names:
                        field_dictionary[field_set][field_set + "." + field] = []
                # Populate dictionary for field set
                field_dictionary[field_set]['extent_id'] += numpy.uint64(rsp['index']),
                for field in field_names:
                    if as_numpy_array:
                        data = []
                        for s in rsp["samples"]:
                            data += s[field],
                        field_dictionary[field_set][field_set + "." + field] += numpy.array(data),
                    else:
                        field_dictionary[field_set][field_set + "." + field] += sample[field],
            elif 'waverec' == rsp['__rectype']:
                field_set = rsp['__rectype']
                field_names = list(rsp.keys())
                field_names.remove("__rectype")
                if field_set not in field_dictionary:
                    field_dictionary[field_set] = {'extent_id': []}
                    for field in field_names:
                        field_dictionary[field_set][field] = []
                for field in field_names:
                    if type(rsp[field]) == tuple:
                        field_dictionary[field_set][field] += numpy.array(rsp[field]),
                    elif field == 'extent_id':
                        field_dictionary[field_set][field] += numpy.uint64(rsp[field]),
                    else:
                        field_dictionary[field_set][field] += rsp[field],
            elif 'fileidrec' == rsp['__rectype']:
                file_dictionary[rsp["file_id"]] = rsp["file_name"]

        # Build Columns
        if num_records > 0:
            # Initialize Columns
            sample_record = records[0][batch_column][0]
            for field in sample_record.keys():
                fielddef = sliderule.getdefinition(sample_record['__rectype'], field)
                if len(fielddef) > 0:
                    if type(sample_record[field]) == tuple:
                        columns[field] = numpy.empty(num_records, dtype=object)
                    else:
                        columns[field] = numpy.empty(num_records, fielddef["nptype"])
            # Populate Columns
            cnt = 0
            for record in records:
                for batch in record[batch_column]:
                    for field in columns:
                        columns[field][cnt] = batch[field]
                    cnt += 1
    else:
        logger.debug("No response returned")

    # Build Initial GeoDataFrame
    gdf = sliderule.todataframe(columns, height_key=height_key, crs=ICESAT2_CRS)

    # Merge Ancillary Fields
    for field_set in field_dictionary:
        df = geopandas.pd.DataFrame(field_dictionary[field_set])
        gdf = geopandas.pd.merge(gdf, df, how='left', on='extent_id').set_axis(gdf.index)

    # Delete Extent ID Column
    if len(gdf) > 0 and not keep_id:
        del gdf["extent_id"]

    # Attach Metadata
    if len(file_dictionary) > 0:
        gdf.attrs['file_directory'] = file_dictionary

    # Return GeoDataFrame
    return gdf

#
# Flatten Batches - ATL03
#
def __flattenbatches03(rsps, parm, keep_id, height_key):

    # Check for Responses
    if rsps == None:
        return sliderule.emptyframe(crs=ICESAT2_CRS)

    # Check for Output Options
    if "output" in parm:
        return sliderule.procoutputfile(parm, rsps)

    else: # Native Output
        # Flatten Responses
        columns = {}
        sample_photon_record = None
        photon_records = []
        num_photons = 0
        photon_dictionary = {}
        photon_field_types = {} # ['field_name'] = nptype
        if len(rsps) > 0:
            # Sort Records
            for rsp in rsps:
                if 'atl03rec' in rsp['__rectype']:
                    photon_records += rsp,
                    num_photons += len(rsp['photons'])
                    if sample_photon_record == None and len(rsp['photons']) > 0:
                        sample_photon_record = rsp
                elif 'ancerec' == rsp['__rectype']:
                    # Get Field Name and Type
                    field_name = __getancillaryfield(parm, rsp)
                    if field_name not in photon_field_types:
                        photon_field_types[field_name] = BASIC_TYPES[CODED_TYPE[rsp['datatype']]]["nptype"]
                    # Initialize Extent Dictionary Entry
                    extent_id = rsp['extent_id']
                    if extent_id not in photon_dictionary:
                        photon_dictionary[extent_id] = {}
                    # Save of Values per Extent ID per Field Name
                    data = sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']))
                    photon_dictionary[extent_id][field_name] = data
            # Build Elevation Columns
            if num_photons > 0:
                # Initialize Columns
                for field in sample_photon_record.keys():
                    fielddef = sliderule.getdefinition("atl03rec", field)
                    if len(fielddef) > 0 and field != "photons":
                        columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                for field in sample_photon_record["photons"][0].keys():
                    fielddef = sliderule.getdefinition("atl03rec.photons", field)
                    if len(fielddef) > 0:
                        columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                for field in photon_field_types.keys():
                    columns[field] = numpy.empty(num_photons, photon_field_types[field])
                # Populate Columns
                ph_cnt = 0
                for record in photon_records:
                    # Add Ancillary Extent Fields
                    ph_index = 0
                    extent_id = record['extent_id']
                    if extent_id in photon_dictionary:
                        for photon in record["photons"]:
                            for field_name, field_array in photon_dictionary[extent_id].items():
                                columns[field_name][ph_cnt + ph_index] = field_array[ph_index]
                            ph_index += 1
                    # For Each Photon in Extent
                    for photon in record["photons"]:
                        # Add per Extent Fields
                        for field in record.keys():
                            if field in columns:
                                columns[field][ph_cnt] = record[field]
                        # Add per Photon Fields
                        for field in photon.keys():
                            if field in columns:
                                columns[field][ph_cnt] = photon[field]
                        # Goto Next Photon
                        ph_cnt += 1

                # Delete Extent ID Column
                if "extent_id" in columns and not keep_id:
                    del columns["extent_id"]

                # Create DataFrame
                gdf = sliderule.todataframe(columns, height_key=height_key, crs=ICESAT2_CRS)

                # Calculate Spot Column
                gdf['spot'] = __calcspot(gdf)

                # Return Response
                return gdf
            else:
                logger.debug("No photons returned")
        else:
            logger.debug("No response returned")

    # Return Empty Response
    return sliderule.emptyframe(crs=ICESAT2_CRS)

#
# Flatten Batches - ATL03 Viewer
#
def __flattenbatches03v(rsps, parm, keep_id):

    # Check for Responses
    if rsps == None:
        return sliderule.emptyframe(crs=ICESAT2_CRS)

    # Check for Output Options
    if "output" in parm:
        return sliderule.procoutputfile(parm, rsps)

    else: # Native Output
        # Flatten Responses
        columns = {}
        sample_segment_record = None
        extent_records = []
        num_segments = 0
        if len(rsps) > 0:
            # Sort Records
            for rsp in rsps:
                if 'atl03vrec' in rsp['__rectype']:
                    extent_records += rsp,
                    num_segments += len(rsp['segments'])
                    if sample_segment_record == None and len(rsp['segments']) > 0:
                        sample_segment_record = rsp
            # Build Segments Columns
            if num_segments > 0:
                # Initialize Columns
                for field in sample_segment_record.keys():
                    fielddef = sliderule.getdefinition("atl03vrec", field)
                    if len(fielddef) > 0 and field != "segments":
                        columns[field] = numpy.empty(num_segments, fielddef["nptype"])
                for field in sample_segment_record["segments"][0].keys():
                    fielddef = sliderule.getdefinition("atl03vrec.segments", field)
                    if len(fielddef) > 0:
                        columns[field] = numpy.empty(num_segments, fielddef["nptype"])
                # Populate Columns
                seg_cnt = 0
                for record in extent_records:
                    # For Each Segment in Extent
                    for segment in record["segments"]:
                        # Add per Extent Fields
                        for field in record.keys():
                            if field in columns:
                                columns[field][seg_cnt] = record[field]
                        # Add per Segments Fields
                        for field in segment.keys():
                            if field in columns:
                                columns[field][seg_cnt] = segment[field]
                        # Goto Next Segment
                        seg_cnt += 1

                # Delete Extent ID Column
                if "extent_id" in columns and not keep_id:
                    del columns["extent_id"]

                # Create DataFrame
                gdf = sliderule.todataframe(columns, crs=ICESAT2_CRS)

                # Return Response
                return gdf
            else:
                logger.debug("No segment counts returned")
        else:
            logger.debug("No response returned")

    # Error Case
    return sliderule.emptyframe(crs=ICESAT2_CRS)

#
# Build Request
#
def __build_request(parm, resources, default_asset='icesat2'):

    # Default the Asset
    rqst_parm = parm.copy()
    if "asset" not in rqst_parm:
        rqst_parm["asset"] = default_asset

    # Build Request
    return {
        "resources": resources,
        "parms": rqst_parm
    }
