/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University of Washington nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Usgs3dep10meterDemRaster.h"


/*----------------------------------------------------------------------------
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
OGRErr Usgs3dep10meterDemRaster::overrideTargetCRS(OGRSpatialReference& target, const void* param)
{
    static_cast<void>(param);
    OGRErr ogrerr = OGRERR_FAILURE;
    const int HORIZONTAL_EPSG = 4269;  // NAD83
    const int VERTICAL_EPSG   = 5703;  // NAVD88 height

    const int input_epsg = target.GetEPSGGeogCS();
    if (input_epsg != HORIZONTAL_EPSG)
    {
        mlog(ERROR, "Unsupported CRS EPSG:%d, only EPSG:4269 is supported for 10m tiles", input_epsg);
        return ogrerr;
    }

    OGRSpatialReference horizontal;
    OGRSpatialReference vertical;

    const OGRErr er1 = horizontal.importFromEPSG(HORIZONTAL_EPSG);
    const OGRErr er2 = vertical.importFromEPSG(VERTICAL_EPSG);
    const OGRErr er3 = target.SetCompoundCS("sliderule", &horizontal, &vertical);

    if(er1 == OGRERR_NONE && er2 == OGRERR_NONE && er3 == OGRERR_NONE)
    {
        mlog(DEBUG, "Successfully constructed compound CRS EPSG:%d+%d", HORIZONTAL_EPSG, VERTICAL_EPSG);
        ogrerr = OGRERR_NONE;
    }
    else mlog(ERROR, "Failed to overried target CRS");

    return ogrerr;
}
