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
import logging
import numpy
import geopandas
import sliderule
from sliderule import earthdata

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# profiling times for each major function
profiles = {}

# default assets
DEFAULT_L1B_ASSET="gedil1b"
DEFAULT_L2A_ASSET="gedil2a"
DEFAULT_L4A_ASSET="gedil4a"

# default GEDI standard data product version
DEFAULT_GEDI_SDP_VERSION = '2'

# gedi parameters
ALL_BEAMS = -1

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
#  Dictionary to GeoDataFrame
#
def __todataframe(columns, time_key="time", lon_key="longitude", lat_key="latitude", **kwargs):

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
    field_dictionary = {} # [<field_name>] = {"shot_number": [], <field_name>: []}
    file_dictionary = {} # [id] = "filename"
    if len(rsps) > 0:
        # Sort Records
        for rsp in rsps:
            if rectype in rsp['__rectype']:
                records += rsp,
                num_records += len(rsp[batch_column])
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
                    field_dictionary[field_set] = {'shot_number': []}
                    for field in field_names:
                        field_dictionary[field_set][field_set + "." + field] = []
                # Populate dictionary for field set
                field_dictionary[field_set]['shot_number'] += numpy.uint64(rsp['index']),
                for field in field_names:
                    if as_numpy_array:
                        data = []
                        for s in rsp["samples"]:
                            data += s[field],
                        field_dictionary[field_set][field_set + "." + field] += numpy.array(data),
                    else:
                        field_dictionary[field_set][field_set + "." + field] += sample[field],
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
        gdf = geopandas.pd.merge(gdf, df, how='left', on='shot_number').set_axis(gdf.index)
    profiles["merge"] = time.perf_counter() - tstart_merge

    # Delete Shot Number Column
    if len(gdf) > 0 and not keep_id:
        del gdf["shot_number"]

    # Attach Metadata
    if len(file_dictionary) > 0:
        gdf.attrs['file_directory'] = file_dictionary

    # Return GeoDataFrame
    profiles["flatten"] = time.perf_counter() - tstart_flatten
    return gdf

#
#  Query Resources from CMR
#
def __query_resources(parm, dataset, **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Check Parameters are Valid
    if ("poly" not in parm) and ("t0" not in parm) and ("t1" not in parm):
        logger.error("Must supply some bounding parameters with request (poly, t0, t1)")
        return []

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

    # Make CMR Request
    if kwargs['return_metadata']:
        resources,metadata = earthdata.cmr(short_name=dataset, **kwargs)
    else:
        resources = earthdata.cmr(short_name=dataset, **kwargs)

    # Check Resources are Under Limit
    if(len(resources) > earthdata.max_requested_resources):
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

#
#  Perform Processing Request
#
def __processing_request(parm, asset, callbacks, resources, keep_id, dataset, api, rec, profile):
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not supplied)
        if resources == None:
            resources = __query_resources(parm, dataset)

        # Build GEDI Request
        parm["asset"] = asset
        rqst = {
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source(api, rqst, stream=True, callbacks=callbacks)

        # Flatten Responses
        gdf = __flattenbatches(rsps, rec, 'footprint', parm, keep_id)

        # Return Response
        profiles[profile] = time.perf_counter() - tstart
        return gdf

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return sliderule.emptyframe()

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=sliderule.service_url, verbose=False, max_resources=earthdata.DEFAULT_MAX_REQUESTED_RESOURCES, loglevel=logging.CRITICAL, organization=sliderule.service_org, desired_nodes=None, time_to_live=60, bypass_dns=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other GEDI API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Parameters
    ----------
        max_resources:  int
                        maximum number of H5 granules to process in the request

    Examples
    --------
        >>> from sliderule import gedi
        >>> gedi.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns, plugins=['gedi'])
    earthdata.set_max_resources(max_resources) # set maximum number of resources allowed per request

#
#  GEDI L4A
#
def gedi04a (parm, resource, asset=DEFAULT_L4A_ASSET):
    '''
    Performs GEDI L4A subsetting of elevation footprints

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
#  Parallel GEDI04A
#
def gedi04ap(parm, asset=DEFAULT_L4A_ASSET, callbacks={}, resources=None, keep_id=False):
    '''
    Performs subsetting in parallel on GEDI data and returns elevation footprints.  This function expects that the **parm** argument
    includes a polygon which is used to fetch all available resources from the CMR system automatically.  If **resources** is specified
    then any polygon or resource filtering options supplied in **parm** are ignored.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure subsetting process
        asset:          str
                        data source asset
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        geolocated footprints

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
    return __processing_request(parm, asset, callbacks, resources, keep_id, 'GEDI_L4A_AGB_Density_V2_1_2056', 'gedi04ap', 'gedi04arec', gedi04ap.__name__)

#
#  GEDI L2A
#
def gedi02a (parm, resource, asset=DEFAULT_L2A_ASSET):
    '''
    Performs GEDI L2A subsetting of elevation footprints

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
    return gedi02ap(parm, asset=asset, resources=[resource])

#
#  Parallel GEDI02A
#
def gedi02ap(parm, asset=DEFAULT_L2A_ASSET, callbacks={}, resources=None, keep_id=False):
    '''
    Performs subsetting in parallel on GEDI data and returns geolocated footprints.  This function expects that the **parm** argument
    includes a polygon which is used to fetch all available resources from the CMR system automatically.  If **resources** is specified
    then any polygon or resource filtering options supplied in **parm** are ignored.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure subsetting process
        asset:          str
                        data source asset
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        geolocated footprints

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
        >>> resources = ["GEDI02_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5"]
        >>> asset = "gedi-local"
        >>> rsps = gedi.gedi02ap(parms, asset=asset, resources=resources)
    '''
    return __processing_request(parm, asset, callbacks, resources, keep_id, 'GEDI02_A', 'gedi02ap', 'gedi02arec', gedi02ap.__name__)

#
#  GEDI L1B
#
def gedi01b (parm, resource, asset=DEFAULT_L1B_ASSET):
    '''
    Performs GEDI L1B subsetting of elevation waveforms

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
    return gedi01bp(parm, asset=asset, resources=[resource])

#
#  Parallel GEDI01B
#
def gedi01bp(parm, asset=DEFAULT_L1B_ASSET, callbacks={}, resources=None, keep_id=False):
    '''
    Performs subsetting in parallel on GEDI data and returns geolocated footprints.  This function expects that the **parm** argument
    includes a polygon which is used to fetch all available resources from the CMR system automatically.  If **resources** is specified
    then any polygon or resource filtering options supplied in **parm** are ignored.

    Parameters
    ----------
        parms:          dict
                        parameters used to configure subsetting process
        asset:          str
                        data source asset
        callbacks:      dictionary
                        a callback function that is called for each result record
        resources:      list
                        a list of granules to process (e.g. ["GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5", ...])
        keep_id:        bool
                        whether to retain the "extent_id" column in the GeoDataFrame for future merges

    Returns
    -------
    GeoDataFrame
        geolocated footprints

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
        >>> resources = ["GEDI01_B_2019229131935_O03846_02_T03642_02_002_02_V002.h5"]
        >>> asset = "gedi-local"
        >>> rsps = gedi.gedi01bp(parms, asset=asset, resources=resources)
    '''
    return __processing_request(parm, asset, callbacks, resources, keep_id, 'GEDI01_B', 'gedi01bp', 'gedi01brec', gedi01bp.__name__)
