# Copyright (c) 2023, University of Texas
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
# 3. Neither the name of the University of Texas nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
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
from sliderule import earthdata, icesat2

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# profiling times for each major function
profiles = {}

OPTION_0 = 0
OPTION_1 = 1
OPTION_2 = 2

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

#
# Build Request
#
def __build_request(parm, resources):

    # Default the Asset
    if "asset" not in parm:
        parm["asset"] = "icesat2"

    # Get List of Resources
    resources = earthdata.search(parm, resources)

    # Build Request
    return {
        "resources": resources,
        "parms": parm
    }


###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=sliderule.service_url, verbose=False, loglevel=logging.CRITICAL, organization=sliderule.service_org, desired_nodes=None, time_to_live=60, bypass_dns=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other Bathy API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Examples
    --------
        >>> from sliderule import bathy
        >>> bathy.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns, plugins=['bathy'])

#
#  ATL24
#
def atl24 (parm, resource):
    '''
    Execute ATL24 bathymetry algorithm and return segments of classified photons

    Parameters
    ----------
        parms:      dict
                    parameters used to configure ATL24
        resource:   str
                    ATL03 HDF5 filename

    Returns
    -------
    GeoDataFrame
        ATL24 extents
    '''
    return atl24p(parm, resources=[resource])

#
#  Parallel ATL24
#
def atl24p(parm, callbacks={}, resources=None, keep_id=False, height_key=None):
    '''
    Performs ATL24 bathymetry algorithm in parallel on ATL03 data and returns classified photon segment data.  Unlike the `atl24 <#atl24>`_ function,
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
        ATL24 segments
    '''
    try:
        tstart = time.perf_counter()

        # Build Request
        rqst = __build_request(parm, resources)

        # Make Request
        rsps = sliderule.source("atl24p", rqst, stream=True, callbacks=callbacks)

        # Check for Output Options
        if "output" in parm:
            profiles[atl24p.__name__] = time.perf_counter() - tstart
            return sliderule.procoutputfile(parm)
        else: # Native Output
            # Flatten Responses
            tstart_flatten = time.perf_counter()
            columns = {}
            sample_photon_record = None
            photon_records = []
            num_photons = 0
            photon_field_types = {} # ['field_name'] = nptype
            if len(rsps) > 0:
                # Sort Records
                for rsp in rsps:
                    if 'atl24rec' in rsp['__rectype']:
                        photon_records += rsp,
                        num_photons += len(rsp['photons'])
                        if sample_photon_record == None and len(rsp['photons']) > 0:
                            sample_photon_record = rsp
                # Build Elevation Columns
                if num_photons > 0:
                    # Initialize Columns
                    for field in sample_photon_record.keys():
                        fielddef = sliderule.get_definition("atl24rec", field)
                        if len(fielddef) > 0 and field != "photons":
                            columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                    for field in sample_photon_record["photons"][0].keys():
                        fielddef = sliderule.get_definition("atl24rec.photons", field)
                        if len(fielddef) > 0:
                            columns[field] = numpy.empty(num_photons, fielddef["nptype"])
                    for field in photon_field_types.keys():
                        columns[field] = numpy.empty(num_photons, photon_field_types[field])
                    # Populate Columns
                    ph_cnt = 0
                    for record in photon_records:
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

                    # Capture Time to Flatten
                    profiles["flatten"] = time.perf_counter() - tstart_flatten

                    # Create DataFrame
                    gdf = sliderule.todataframe(columns, height_key=height_key)

                    # Calculate Spot Column
                    gdf['spot'] = gdf.apply(lambda row: icesat2.__calcspot(row["sc_orient"], row["track"], row["pair"]), axis=1)

                    # Return Response
                    profiles[atl24p.__name__] = time.perf_counter() - tstart
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
