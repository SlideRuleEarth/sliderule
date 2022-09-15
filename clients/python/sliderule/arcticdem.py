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
import sliderule

###############################################################################
# GLOBALS
###############################################################################

# create logger
logger = logging.getLogger(__name__)

# default asset
DEFAULT_ASSET="arcticdem-s3"

###############################################################################
# APIs
###############################################################################

#
#  Get Elevation
#
def elevation (coordinate, asset=DEFAULT_ASSET):
    '''
    Get elevation from ArcticDEM at provided coordinate

    Parameters
    ----------
        coordinate :    list
                        [<longitude>, <latitude>]
        asset:          str
                        data source asset (see `Assets <../user_guide/ArcticDEM.html#assets>`_)

    Examples
    --------
        >>> from sliderule import arcticdem
        >>> arcticdem.elevation([23.14333, 70.3211])
    '''
    return elevations([coordinate])

#
#  Get Elevations
#
def elevations (coordinates, asset=DEFAULT_ASSET):
    '''
    Get elevations from ArcticDEM at provided coordinates

    Parameters
    ----------
        coordinates :   list
                        [[<longitude>, <latitude>], [<longitude>, <latitude>], ... ]
        asset:          str
                        data source asset (see `Assets <../user_guide/ArcticDEM.html#assets>`_)

    Examples
    --------
        >>> from sliderule import arcticdem
        >>> arcticdem.elevations([[164.134, 73.9291], [23.14333, 70.3211]])
    '''
    # Build Request
    rqst = {
        "dem-asset" : asset,
        "coordinates": coordinates
    }

    # Make API Processing Request
    rsps = sliderule.source("elevation", rqst, stream=False)

    # Return Response
    return rsps
