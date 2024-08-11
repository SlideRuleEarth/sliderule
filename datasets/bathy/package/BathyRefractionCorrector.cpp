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

#include <cmath>
#include <numeric>
#include <algorithm>

#include "OsApi.h"
#include "GeoLib.h"
#include "BathyRefractionCorrector.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyRefractionCorrector::OBJECT_TYPE = "BathyRefractionCorrector";
const char* BathyRefractionCorrector::LUA_META_NAME = "BathyRefractionCorrector";
const struct luaL_Reg BathyRefractionCorrector::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathyRefractionCorrector::luaCreate (lua_State* L)
{
    BathyParms* parms = NULL;

    try
    {
        parms = dynamic_cast<BathyParms*>(getLuaObject(L, 1, BathyParms::OBJECT_TYPE));
        return createLuaObject(L, new BathyRefractionCorrector(L, parms));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void BathyRefractionCorrector::init (void)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyRefractionCorrector::BathyRefractionCorrector (lua_State* L, BathyParms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyRefractionCorrector::~BathyRefractionCorrector (void)
{
}

/*----------------------------------------------------------------------------
 * photon_refraction -
 *
 * ICESat-2 refraction correction implemented as outlined in Parrish, et al.
 * 2019 for correcting photon depth data. Reference elevations are to geoid datum
 * to remove sea surface variations.
 *
 * https://www.mdpi.com/2072-4292/11/14/1634
 *
 * ----------------------------------------------------------------------------
 * The code below was adapted from https://github.com/ICESat2-Bathymetry/Information.git
 * with the associated license replicated here:
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2022, Jonathan Markel/UT Austin.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR '
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------------*/
uint64_t BathyRefractionCorrector::run( BathyParms::extent_t& extent, 
                                        const H5Array<float>& ref_el,
                                        const H5Array<float>& ref_az ) const
{
    uint64_t subaqueous_photons = 0;

    GeoLib::UTMTransform transform(extent.utm_zone, extent.region < 8);

    BathyParms::photon_t* photons = extent.photons;
    for(uint32_t i = 0; i < extent.photon_count; i++)
    {
        const int32_t seg = extent.photons[i].index_seg;
        const double depth = photons[i].surface_h - photons[i].ortho_h; // compute un-refraction-corrected depths
        if(depth > 0)
        {
            /* Count Subaqueous Photons */
            subaqueous_photons++;

            /* Calculate Refraction Corrections */
            const double n1 = parms->refraction.ri_air;
            const double n2 = parms->refraction.ri_water;
            const double theta_1 = (M_PI / 2.0) - ref_el[seg];              // angle of incidence (without Earth curvature)
            const double theta_2 = asin(n1 * sin(theta_1) / n2);            // angle of refraction
            const double phi = theta_1 - theta_2;
            const double s = depth / cos(theta_1);                          // uncorrected slant range to the uncorrected seabed photon location
            const double r = s * n1 / n2;                                   // corrected slant range
            const double p = sqrt((r*r) + (s*s) - (2*r*s*cos(theta_1 - theta_2)));
            const double gamma = (M_PI / 2.0) - theta_1;
            const double alpha = asin(r * sin(phi) / p);
            const double beta = gamma - alpha;
            const double dZ = p * sin(beta);                                // vertical offset
            const double dY = p * cos(beta);                                // cross-track offset
            const double dE = dY * sin(static_cast<double>(ref_az[seg]));   // UTM offsets
            const double dN = dY * cos(static_cast<double>(ref_az[seg])); 

            /* Save Refraction Height Correction */
            photons[i].delta_h = dZ;

            /* Correct Latitude and Longitude */
            double corr_x_ph = photons[i].x_ph + dE;
            double corr_y_ph = photons[i].y_ph + dN;
            const GeoLib::point_t point = transform.calculateCoordinates(corr_x_ph, corr_y_ph);
            photons[i].lat_ph = point.x;
            photons[i].lon_ph = point.y;
        }
    }

    return subaqueous_photons;
}
