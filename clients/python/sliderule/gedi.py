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
from sliderule import logger
from sliderule.session import Session

###############################################################################
# GLOBAL DATA
###############################################################################

# CRS used in GEDI Standard Data Products
GEDI_CRS = "EPSG:7912"

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
# Flatten Batches
#
def __flattenbatches(rsps, rectype, batch_column, parm, keep_id, as_numpy_array, height_key):

    # Check Responses
    if rsps == None:
        return sliderule.emptyframe(crs=GEDI_CRS)

    # Check for Output Options
    if "output" in parm:
        gdf = sliderule.procoutputfile(parm, rsps)
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
            elif 'ancfrec' == rsp['__rectype']:
                for field_rec in rsp['fields']:
                    shot_number = numpy.uint64(rsp['extent_id'])
                    field_name = parm['anc_fields'][field_rec['field_index']]
                    if field_name not in field_dictionary:
                        field_dictionary[field_name] = {'shot_number': [], field_name: []}
                    field_dictionary[field_name]['shot_number'] += shot_number,
                    field_dictionary[field_name][field_name] += sliderule.getvalues(field_rec['value'], field_rec['datatype'], len(field_rec['value']), num_elements=1)[0],
            elif 'ancerec' == rsp['__rectype']:
                shot_number = numpy.uint64(rsp['extent_id'])
                field_name = parm['anc_fields'][rsp['field_index']]
                if field_name not in field_dictionary:
                    field_dictionary[field_name] = {'shot_number': [], field_name: []}
                field_dictionary[field_name]['shot_number'] += shot_number,
                field_dictionary[field_name][field_name] += sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']), num_elements=rsp['num_elements']),
            elif 'rsrec' == rsp['__rectype'] or 'zsrec' == rsp['__rectype']:
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
    gdf = sliderule.todataframe(columns, height_key=height_key, crs=GEDI_CRS)

    # Merge Ancillary Fields
    for field_set in field_dictionary:
        df = geopandas.pd.DataFrame(field_dictionary[field_set])
        gdf = geopandas.pd.merge(gdf, df, how='left', on='shot_number').set_axis(gdf.index)

    # Delete Shot Number Column
    if len(gdf) > 0 and not keep_id:
        del gdf["shot_number"]

    # Attach Metadata
    if len(file_dictionary) > 0:
        gdf.attrs['file_directory'] = file_dictionary

    # Return GeoDataFrame
    return gdf

#
#  Perform Processing Request
#
def __processing_request(parm, asset, callbacks, resources, keep_id, as_numpy_array, api, rec, height_key):

    # Default the Asset
    rqst_parm = parm.copy()
    if "asset" not in rqst_parm:
        rqst_parm["asset"] = asset

    # Build GEDI Request
    rqst = {
        "resources": resources,
        "parms": rqst_parm
    }

    # Make API Processing Request
    rsps = sliderule.source(api, rqst, stream=True, callbacks=callbacks)

    # Return GeoDataFrame
    return __flattenbatches(rsps, rec, 'footprint', parm, keep_id, as_numpy_array, height_key)

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=Session.PUBLIC_URL, verbose=False, loglevel=logging.CRITICAL, organization=Session.PUBLIC_ORG, desired_nodes=None, time_to_live=60, bypass_dns=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other GEDI API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Examples
    --------
        >>> from sliderule import gedi
        >>> gedi.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns)

#
#  GEDI L4A
#
def gedi04a (parm, resource):
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
    return gedi04ap(parm, resources=[resource])

#
#  Parallel GEDI04A
#
def gedi04ap(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
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
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

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
    return __processing_request(parm, "gedil4a", callbacks, resources, keep_id, as_numpy_array, 'gedi04ap', 'gedi04arec', height_key)

#
#  GEDI L2A
#
def gedi02a (parm, resource):
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
    return gedi02ap(parm, resources=[resource])

#
#  Parallel GEDI02A
#
def gedi02ap(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
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
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

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
    return __processing_request(parm, "gedil2a", callbacks, resources, keep_id, as_numpy_array, 'gedi02ap', 'gedi02arec', height_key)

#
#  GEDI L1B
#
def gedi01b (parm, resource):
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
    return gedi01bp(parm, resources=[resource])

#
#  Parallel GEDI01B
#
def gedi01bp(parm, callbacks={}, resources=None, keep_id=False, as_numpy_array=False, height_key=None):
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
        as_numpy_array: bool
                        whether to provide all sampled values as numpy arrays even if there is only a single value
        height_key:     str
                        identifies the name of the column provided for the 3D CRS transformation

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
    return __processing_request(parm, "gedil1b", callbacks, resources, keep_id, as_numpy_array, 'gedi01bp', 'gedi01brec', height_key)
