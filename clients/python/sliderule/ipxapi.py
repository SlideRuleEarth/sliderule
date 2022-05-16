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

from sliderule import icesat2
import logging

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

###############################################################################
# APIs
###############################################################################

#
#  ICEPYX ATL06
#
def atl06p(ipx_region, parm, asset=icesat2.DEFAULT_ASSET):
    """
    Performs ATL06-SR processing in parallel on ATL03 data and returns gridded elevations.  The list of granules to be processed is identified by the ipx_region object.

    See the `atl06p <../api_reference/icesat2.html#atl06p>`_ function for more details.

    Parameters
    ----------
        ipx_region: Query
                    icepyx region object defining the query of granules to be processed
        parm:       dict
                    parameters used to configure ATL06-SR algorithm processing (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
        asset:      str
                    data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    GeoDataFrame
        gridded elevations (see `Elevations <../user_guide/ICESat-2.html#elevations>`_)
    """
    try:
        version = ipx_region.product_version
        resources = ipx_region.avail_granules(ids=True)[0]
    except:
        logger.critical("must supply an icepyx query as region")
        return icesat2.__emptyframe()
    # try to get the subsetting region
    if ipx_region.extent_type in ('bbox','polygon'):
        parm.update({'poly': to_region(ipx_region)})

    return icesat2.atl06p(parm, asset, version=version, resources=resources)

#
#  ICEPYX ATL03
#
def atl03sp(ipx_region, parm, asset=icesat2.DEFAULT_ASSET):
    """
    Performs ATL03 subsetting in parallel on ATL03 data and returns photon segment data.

    See the `atl03sp <../api_reference/icesat2.html#atl03sp>`_ function for more details.

    Parameters
    ----------
        ipx_region: Query
                    icepyx region object defining the query of granules to be processed
        parms:      dict
                    parameters used to configure ATL03 subsetting (see `Parameters <../user_guide/ICESat-2.html#parameters>`_)
        asset:      str
                    data source asset (see `Assets <../user_guide/ICESat-2.html#assets>`_)

    Returns
    -------
    list
        ATL03 segments (see `Photon Segments <../user_guide/ICESat-2.html#photon-segments>`_)
    """
    try:
        version = ipx_region.product_version
        resources = ipx_region.avail_granules(ids=True)[0]
    except:
        logger.critical("must supply an icepyx query as region")
        return icesat2.__emptyframe()
    # try to get the subsetting region
    if ipx_region.extent_type in ('bbox','polygon'):
        parm.update({'poly': to_region(ipx_region)})

    return icesat2.atl03sp(parm, asset, version=version, resources=resources)

def to_region(ipx_region):
    """
    Extract subsetting extents from an icepyx region

    Parameters
    ----------
        ipx_region: Query
                    icepyx region object defining the query of granules to be processed

    Returns
    -------
    list
        polygon definining region of interest (can be passed into `icesat2` api functions)

    """
    if (ipx_region.extent_type == 'bbox'):
        bbox = ipx_region.spatial_extent[1]
        poly = [dict(lon=bbox[0], lat=bbox[1]),
                dict(lon=bbox[2], lat=bbox[1]),
                dict(lon=bbox[2], lat=bbox[3]),
                dict(lon=bbox[0], lat=bbox[3]),
                dict(lon=bbox[0], lat=bbox[1])]
    elif (ipx_region.extent_type == 'polygon'):
        poly = [dict(lon=ln,lat=lt) for ln,lt in zip(*ipx_region.spatial_extent[1])]
    return poly
