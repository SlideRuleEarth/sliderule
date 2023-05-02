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
from sliderule import earthdata
from sliderule import h5 as h5coro

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# profiling times for each major function
profiles = {}

# default asset
DEFAULT_ASSET="icesat2"

# default standard data product version
DEFAULT_ICESAT2_SDP_VERSION='005'

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
#  Dictionary to GeoDataFrame
#
def __todataframe(columns, time_key="time", lon_key="lon", lat_key="lat", **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Set Default Keyword Arguments
    kwargs['index_key'] = "time"
    kwargs['crs'] = sliderule.EPSG_MERCATOR

    # Check Empty Columns
    if len(columns) <= 0:
        return sliderule.emptyframe(**kwargs)

    # Generate Time Column
    columns['time'] = columns[time_key].astype('datetime64[ns]')

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
# Flatten Batches
#
def __flattenbatches(rsps, rectype, batch_column, parm, keep_id):

    # Latch Start Time
    tstart_flatten = time.perf_counter()

    # Check for Output Options
    if "output" in parm:
        gdf = sliderule.procoutputfile(parm)
        profiles["flatten"] = time.perf_counter() - tstart_flatten
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
            elif 'extrec' == rsp['__rectype']:
                field_name = parm['atl03_geo_fields'][rsp['field_index']]
                if field_name not in field_dictionary:
                    field_dictionary[field_name] = {'extent_id': [], field_name: []}
                # Parse Ancillary Data
                data = sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']))
                # Add Left Pair Track Entry
                field_dictionary[field_name]['extent_id'] += numpy.uint64(rsp['extent_id']) | numpy.uint64(0x2),
                field_dictionary[field_name][field_name] += data[LEFT_PAIR],
                # Add Right Pair Track Entry
                field_dictionary[field_name]['extent_id'] += numpy.uint64(rsp['extent_id']) | numpy.uint64(0x3),
                field_dictionary[field_name][field_name] += data[RIGHT_PAIR],
            elif 'rsrec' == rsp['__rectype'] or 'zsrec' == rsp['__rectype']:
                if rsp["num_samples"] <= 0:
                    continue
                # Get field names and set
                sample = rsp["samples"][0]
                field_names = list(sample.keys())
                field_names.remove("__rectype")
                field_set = rsp['key']
                as_numpy_array = False
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
                fielddef = sliderule.get_definition(sample_record['__rectype'], field)
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
    gdf = __todataframe(columns)

    # Merge Ancillary Fields
    tstart_merge = time.perf_counter()
    for field_set in field_dictionary:
        df = geopandas.pd.DataFrame(field_dictionary[field_set])
        gdf = geopandas.pd.merge(gdf, df, how='left', on='extent_id').set_axis(gdf.index)
    profiles["merge"] = time.perf_counter() - tstart_merge

    # Delete Extent ID Column
    if len(gdf) > 0 and not keep_id:
        del gdf["extent_id"]

    # Attach Metadata
    if len(file_dictionary) > 0:
        gdf.attrs['file_directory'] = file_dictionary

    # Return GeoDataFrame
    profiles["flatten"] = time.perf_counter() - tstart_flatten
    return gdf

#
#  Query Resources from CMR
#
def __query_resources(parm, version, **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Submission Arguments for CMR
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
    if "rgt" in parm and parm["rgt"] != None:
        rgt_filter = f'{parm["rgt"]}'.zfill(4)
        name_filter_enabled = True
    cycle_filter = '??'
    if "cycle" in parm and parm["cycle"] != None:
        cycle_filter = f'{parm["cycle"]}'.zfill(2)
        name_filter_enabled = True
    region_filter = '??'
    if "region" in parm and parm["region"] != None:
        region_filter = f'{parm["region"]}'.zfill(2)
        name_filter_enabled = True
    if name_filter_enabled:
        kwargs['name_filter'] = '*_' + rgt_filter + cycle_filter + region_filter + '_*'

    # Check Parameters are Valid
    if (not name_filter_enabled) and ("poly" not in parm) and ("t0" not in parm) and ("t1" not in parm):
        logger.error("Must supply some bounding parameters with request (poly, t0, t1)")
        return []

    # Make CMR Request
    if kwargs['return_metadata']:
        resources,metadata = earthdata.cmr(short_name='ATL03', version=version, **kwargs)
    else:
        resources = earthdata.cmr(short_name='ATL03', version=version, **kwargs)

    # Check Resources are Under Limit
    if(len(resources) > earthdata.max_requested_resources):
        raise sliderule.FatalError('Exceeded maximum requested granules: {} (current max is {})\nConsider using cmr.set_max_resources to set a higher limit.'.format(len(resources), max_requested_resources))
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
def init (url=sliderule.service_url, verbose=False, max_resources=earthdata.DEFAULT_MAX_REQUESTED_RESOURCES, loglevel=logging.CRITICAL, organization=sliderule.service_org, desired_nodes=None, time_to_live=60, bypass_dns=False):
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
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns, plugins=['icesat2'])
    earthdata.set_max_resources(max_resources) # set maximum number of resources allowed per request

#
#  ATL06
#
def atl06 (parm, resource, asset=DEFAULT_ASSET):
    '''
    Performs ATL06-SR processing on ATL03 data and returns geolocated elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtds/user_guide/ICESat-2.html#parameters>`_)
    resource:   str
                ATL03 HDF5 filename
    asset:      str
                data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        geolocated elevations (see `Elevations </web/rtd/user_guide/ICESat-2.html#elevations>`_)
    '''
    return atl06p(parm, asset=asset, resources=[resource])

#
#  Parallel ATL06
#
def atl06p(parm, asset=DEFAULT_ASSET, version=DEFAULT_ICESAT2_SDP_VERSION, callbacks={}, resources=None, keep_id=False):
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
                        parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/ICESat-2.html#parameters>`_)
        asset:          str
                        data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)
        version:        str
                        the version of the ATL03 data to use for processing
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_004_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        geolocated elevations (see `Elevations </web/rtd/user_guide/ICESat-2.html#elevations>`_)

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
        parm["asset"] = asset
        rqst = {
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source("atl06p", rqst, stream=True, callbacks=callbacks)

        # Flatten Responses
        gdf = __flattenbatches(rsps, 'atl06rec', 'elevation', parm, keep_id)

        # Return Response
        profiles[atl06p.__name__] = time.perf_counter() - tstart
        return gdf

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return sliderule.emptyframe()

#
#  Subsetted ATL03
#
def atl03s (parm, resource, asset=DEFAULT_ASSET):
    '''
    Subsets ATL03 data given the polygon and time range provided and returns segments of photons

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/ICESat-2.html#parameters>`_)
        resource:   str
                    ATL03 HDF5 filename
        asset:      str
                    data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        ATL03 extents (see `Photon Segments </web/rtd/user_guide/ICESat-2.html#segmented-photon-data>`_)
    '''
    return atl03sp(parm, asset=asset, resources=[resource])

#
#  Parallel Subsetted ATL03
#
def atl03sp(parm, asset=DEFAULT_ASSET, version=DEFAULT_ICESAT2_SDP_VERSION, callbacks={}, resources=None, keep_id=False):
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
                        parameters used to configure ATL03 subsetting (see `Parameters </web/rtd/user_guide/ICESat-2.html#parameters>`_)
        asset:          str
                        data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)
        version:        str
                        the version of the ATL03 data to return
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_004_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        ATL03 segments (see `Photon Segments </web/rtd/user_guide/ICESat-2.html#photon-segments>`_)
    '''
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not specified)
        if resources == None:
            resources = __query_resources(parm, version)

        # Build ATL03 Subsetting Request
        parm["asset"] = asset
        rqst = {
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source("atl03sp", rqst, stream=True, callbacks=callbacks)

        # Check for Output Options
        if "output" in parm:
            profiles[atl03sp.__name__] = time.perf_counter() - tstart
            return sliderule.procoutputfile(parm)
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
                        data = sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']))
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
                        data = sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']))
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
                    if "extent_id" in columns and not keep_id:
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
    return sliderule.emptyframe()

#
#  ATL08
#
def atl08 (parm, resource, asset=DEFAULT_ASSET):
    '''
    Performs ATL08-PhoREAL processing on ATL03 and ATL08 data and returns geolocated elevations

    Parameters
    ----------
    parms:      dict
                parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtds/user_guide/ICESat-2.html#parameters>`_)
    resource:   str
                ATL03 HDF5 filename
    asset:      str
                data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        geolocated vegatation statistics
    '''
    return atl08p(parm, asset=asset, resources=[resource])

#
#  Parallel ATL08
#
def atl08p(parm, asset=DEFAULT_ASSET, version=DEFAULT_ICESAT2_SDP_VERSION, callbacks={}, resources=None, keep_id=False):
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
                        parameters used to configure ATL06-SR algorithm processing (see `Parameters </web/rtd/user_guide/ICESat-2.html#parameters>`_)
        asset:          str
                        data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)
        version:        str
                        the version of the ATL03 data to use for processing
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["ATL03_20181019065445_03150111_004_01.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        geolocated vegetation statistics
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
        rsps = sliderule.source("atl08p", rqst, stream=True, callbacks=callbacks)

        # Flatten Responses
        gdf = __flattenbatches(rsps, 'atl08rec', 'vegetation', parm, keep_id)

        # Return Response
        profiles[atl08p.__name__] = time.perf_counter() - tstart
        return gdf

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return sliderule.emptyframe()

#
#  Common Metadata Repository
#
def cmr(version=DEFAULT_ICESAT2_SDP_VERSION, short_name='ATL03', **kwargs):
    '''
    DEPRECATED - use earthdata.cmr(...) instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use earthdata.{} instead'.format(cmr.__name__, cmr.__name__), DeprecationWarning, stacklevel=2)
    return earthdata.cmr(short_name=short_name, version=version, **kwargs)

#
#  H5
#
def h5 (dataset, resource, asset=DEFAULT_ASSET, datatype=sliderule.datatypes["DYNAMIC"], col=0, startrow=0, numrows=h5coro.ALL_ROWS):
    '''
    DEPRECATED - use h5.h5(...) instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use h5.{} instead'.format(h5.__name__, h5.__name__), DeprecationWarning, stacklevel=2)
    return h5coro.h5(dataset, resource, asset, datatype, col, startrow, numrows)

#
#  Parallel H5
#
def h5p (datasets, resource, asset=DEFAULT_ASSET):
    '''
    DEPRECATED - use h5.h5p(...) instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use h5.{} instead'.format(h5p.__name__, h5p.__name__), DeprecationWarning, stacklevel=2)
    return h5coro.h5p(datasets, resource, asset)

#
# Format Region Specification
#
def toregion(source, tolerance=0.0, cellsize=0.01, n_clusters=1):
    '''
    DEPRECATED - use sliderule.toregion(...) instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use sliderule.{} instead'.format(toregion.__name__, toregion.__name__), DeprecationWarning, stacklevel=2)
    return sliderule.toregion(source, tolerance, cellsize, n_clusters)

#
# Get Version
#
def get_version ():
    '''
    DEPRECATED - use sliderule.get_version() instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use sliderule.{} instead'.format(get_version.__name__, get_version.__name__), DeprecationWarning, stacklevel=2)
    return sliderule.get_version()

#
#  Set Maximum Resources
#
def set_max_resources (max_resources):
    '''
    DEPRECATED - use cmr.set_max_resources(...) instead
    '''
    warnings.warn('icesat2.{} is deprecated, please use cmr.{} instead'.format(set_max_resources.__name__, set_max_resources.__name__), DeprecationWarning, stacklevel=2)
    return earthdata.set_max_resources(max_resources)
