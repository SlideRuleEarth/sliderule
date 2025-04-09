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
import sliderule
from sliderule import logger
from sliderule.session import Session

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (url=Session.PUBLIC_URL, verbose=False, loglevel=logging.CRITICAL, organization=Session.PUBLIC_ORG, desired_nodes=None, time_to_live=60, bypass_dns=False):
    '''
    Initializes the Python client for use with SlideRule and should be called before other SWOT API calls.
    This function is a wrapper for the `sliderule.init(...) function </web/rtds/api_reference/sliderule.html#init>`_.

    Examples
    --------
        >>> from sliderule import swot
        >>> swot.init()
    '''
    sliderule.init(url, verbose, loglevel, organization, desired_nodes, time_to_live, bypass_dns)

#
#  L2
#
def swotl2 (parm, resource):
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
    return swotl2p(parm, resources=[resource])

#
#  Parallel L2
#
def swotl2p(parm, callbacks={}, resources=None):
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
        # Default the Asset
        rqst_parm = parm.copy()
        if "asset" not in rqst_parm:
            rqst_parm["asset"] = "swot-sim-ecco-llc4320"

        # Build GEDI Request
        rqst = {
            "resources": resources,
            "parms": rqst_parm
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
        return results

    # Handle Runtime Errors
    except RuntimeError as e:
        logger.critical(e)
        return {}
