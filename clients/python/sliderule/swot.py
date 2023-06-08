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
DEFAULT_ASSET="swot-sim-ecco-llc4320"

# asset to dataset
asset2dataset = {
    'swot-sim-ecco-llc4320': "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1",
    'swot-sim-glorys': "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1"
}

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
# Flatten Batches
#
def __flattenbatches(rsps, rectype, batch_column, parm, keep_id, as_numpy_array):

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
    gdf = sliderule.todataframe(columns)

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
    if ("t0" not in parm) and ("t1" not in parm):
        kwargs['time_start'] = None
        kwargs['time_end'] = None
    else:
        if "t0" in parm:
            kwargs['time_start'] = parm["t0"]
        if "t1" in parm:
            kwargs['time_end'] = parm["t1"]

    # Make CMR Request
    if kwargs['return_metadata']:
        resources,metadata = earthdata.cmr(short_name=dataset, **kwargs)
    else:
        resources = earthdata.cmr(short_name=dataset, **kwargs)

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
def init (url=sliderule.service_url, verbose=False, loglevel=logging.CRITICAL, organization=sliderule.service_org, desired_nodes=None, time_to_live=60, bypass_dns=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other SWOT API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Examples
    --------
        >>> from sliderule import swot
        >>> swot.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns, plugins=['swot'])

#
#  L2
#
def swotl2 (parm, resource, asset=DEFAULT_ASSET):
    '''
    Performs L2 subsetting of swaths

    Parameters
    ----------
    parms:      dict
                parameters used to configure subsetting process
    resource:   str
                SWOT NetCDF filename
    asset:      str
                data source asset

    Returns
    -------
    dict
        dictionary of requested datasets keyed by granule name and dataset name
    '''
    return swotl2p(parm, asset=asset, resources=[resource])

#
#  Parallel L2
#
def swotl2p(parm, asset=DEFAULT_ASSET, callbacks={}, resources=None):
    '''
    Performs subsetting in parallel on SWOT  data and returns swaths.  This function expects that the **parm** argument
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
                        a list of granules to process (e.g. ["SWOT_L2_LR_SSH_Expert_009_009_20111121T053342_20111121T062448_DG10_01.nc", ...])

    Returns
    -------
    dict
        dictionary of requested datasets keyed by granule name and dataset name

    Examples
    --------
        >>> from sliderule import swot
        >>> swot.init()
        >>> region = [ {"lon":-105.82971551223244, "lat": 39.81983728534918},
        ...            {"lon":-105.30742121965137, "lat": 39.81983728534918},
        ...            {"lon":-105.30742121965137, "lat": 40.164048017973755},
        ...            {"lon":-105.82971551223244, "lat": 40.164048017973755},
        ...            {"lon":-105.82971551223244, "lat": 39.81983728534918} ]
        >>> parms = { "poly": region }
        >>> resources = ["SWOT_L2_LR_SSH_Expert_009_009_20111121T053342_20111121T062448_DG10_01.nc"]
        >>> asset = "swot-sim-ecco-llc4320"
        >>> rsps = swot.swotl2p(parms, asset=asset, resources=resources)
    '''
    try:
        tstart = time.perf_counter()

        # Get List of Resources from CMR (if not supplied)
        if resources == None:
            resources = __query_resources(parm, asset2dataset[asset])

        # Build GEDI Request
        parm["asset"] = asset
        rqst = {
            "resources": resources,
            "parms": parm
        }

        # Make API Processing Request
        rsps = sliderule.source('swotl2p', rqst, stream=True, callbacks=callbacks)

        # Process Responses
        results = {}
        if len(rsps) > 0:
            for rsp in rsps:
                if 'swotl2var' in rsp['__rectype']:
                    granule = rsp['granule']
                    variable = rsp['variable']
                    if granule not in results:
                        results[granule] = {}
                    if variable not in results[granule]:
                        results[granule][variable] = {}
                    data = sliderule.getvalues(rsp['data'], rsp['datatype'], len(rsp['data']))
                    num_rows = int(rsp['elements'] / rsp['width'])
                    results[granule][variable] = data.reshape(num_rows, rsp['width'])
                elif 'swotl2geo' == rsp['__rectype']:
                    granule = rsp['granule']
                    if granule not in results:
                        results[granule] = {}
                    latitudes = []
                    longitudes = []
                    for point in rsp['scan']:
                        latitudes += point['latitude'],
                        longitudes += point['longitude'],
                    results[granule]['latitude'] = numpy.array(latitudes)
                    results[granule]['longitude'] = numpy.array(longitudes)

        # Return Results
        profiles[swotl2p.__name__] = time.perf_counter() - tstart
        return results

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return {}
