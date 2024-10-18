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
#include "BathyFields.h"
#include "BathyRefractionCorrector.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyRefractionCorrector::GLOBAL_WATER_RI_MASK = "/data/cop_rep_ANNUAL_meanRI_d00.tif";
const double BathyRefractionCorrector::GLOBAL_WATER_RI_MASK_MAX_LAT = 90.0;
const double BathyRefractionCorrector::GLOBAL_WATER_RI_MASK_MIN_LAT = -78.75;
const double BathyRefractionCorrector::GLOBAL_WATER_RI_MASK_MAX_LON = 180.0;
const double BathyRefractionCorrector::GLOBAL_WATER_RI_MASK_MIN_LON = -180.0;
const double BathyRefractionCorrector::GLOBAL_WATER_RI_MASK_PIXEL_SIZE = 0.25;

const char* BathyRefractionCorrector::LUA_META_NAME = "BathyRefractionCorrector";
const struct luaL_Reg BathyRefractionCorrector::LUA_META_TABLE[] = {
    {"subaqueous", getSubAqPh},
    {NULL,  NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathyRefractionCorrector::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;

    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathyRefractionCorrector(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * getSubAqPh - subaqueous(<parms>)
 *----------------------------------------------------------------------------*/
int BathyRefractionCorrector::getSubAqPh (lua_State* L)
{
    try
    {
        BathyRefractionCorrector* lua_obj = dynamic_cast<BathyRefractionCorrector*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->subaqueousPhotons);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting subaqueous photons from %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * sampleWaterMask
 *----------------------------------------------------------------------------*/
double BathyRefractionCorrector::sampleWaterMask (GeoLib::TIFFImage* mask, double lon, double lat)
{
    const double degrees_of_longitude =  lon - GLOBAL_WATER_RI_MASK_MIN_LON;
    const double longitude_pixels = degrees_of_longitude / GLOBAL_WATER_RI_MASK_PIXEL_SIZE;
    const uint32_t x = static_cast<uint32_t>(std::round(longitude_pixels));

    const double degrees_of_latitude = lat - GLOBAL_WATER_RI_MASK_MIN_LAT;
    const double latitude_pixels = degrees_of_latitude / GLOBAL_WATER_RI_MASK_PIXEL_SIZE;
    const uint32_t y = mask->getHeight() - static_cast<uint32_t>(std::round(latitude_pixels)); // flipped image

    const GeoLib::TIFFImage::val_t pixel = mask->getPixel(x, y);
    return pixel.f64;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyRefractionCorrector::BathyRefractionCorrector (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    waterRiMask(NULL),
    subaqueousPhotons(0)
{
    if(parms->refraction.useWaterRIMask)
    {
        waterRiMask = new GeoLib::TIFFImage(NULL, GLOBAL_WATER_RI_MASK, GeoLib::TIFFImage::GDAL_DRIVER);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyRefractionCorrector::~BathyRefractionCorrector (void)
{
    delete waterRiMask;
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run -
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
bool BathyRefractionCorrector::run(GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);
    const RefractionFields& refraction_parms = parms->refraction;

    /* Get UTM Transformation */
    GeoLib::UTMTransform transform(df.utm_zone.value, df.utm_is_north);

    /* Run Refraction Correction */
    for(long i = 0; i < df.length(); i++)
    {
        /* Get Refraction Index of Water */
        double ri_water = refraction_parms.RIWater.value;
        if(waterRiMask)
        {
            const double pixel = sampleWaterMask(waterRiMask, df.lon_ph[i], df.lat_ph[i]);
            if(pixel > 0.0) ri_water = pixel; // only set if valid pixel
        }

        /* Correct All Subaqueous Photons */
        const double depth = df.surface_h[i] - df.geoid_corr_h[i]; // compute un-refraction-corrected depths
        if(depth > 0)
        {
            /* Count Subaqueous Photons */
            subaqueousPhotons++;

            /* Calculate Refraction Corrections */
            const double n1 = refraction_parms.RIAir.value;
            const double n2 = ri_water;
            const double theta_1 = (M_PI / 2.0) - df.ref_el[i];                     // angle of incidence (without Earth curvature)
            const double theta_2 = asin(n1 * sin(theta_1) / n2);                    // angle of refraction
            const double phi = theta_1 - theta_2;
            const double s = depth / cos(theta_1);                                  // uncorrected slant range to the uncorrected seabed photon location
            const double r = s * n1 / n2;                                           // corrected slant range
            const double p = sqrt((r*r) + (s*s) - (2*r*s*cos(theta_1 - theta_2)));
            const double gamma = (M_PI / 2.0) - theta_1;
            const double alpha = asin(r * sin(phi) / p);
            const double beta = gamma - alpha;
            const double dZ = p * sin(beta);                                        // vertical offset
            const double dY = p * cos(beta);                                        // cross-track offset
            const double dE = dY * sin(static_cast<double>(df.ref_az[i]));          // UTM offsets
            const double dN = dY * cos(static_cast<double>(df.ref_az[i]));

            /* Apply Refraction Correction */
#if 0
// to be restored when python code ported to c++
            df.ortho_h[i] = df.ortho_h[i] + dZ;
            df.ellipse_h[i] = df.ellipse_h[i] + dZ;
#else
            df.refracted_dZ[i] = dZ;
#endif

            /* Correct Latitude and Longitude */
            const double corr_x_ph = df.x_ph[i] + dE;
            const double corr_y_ph = df.y_ph[i] + dN;
            const GeoLib::point_t point = transform.calculateCoordinates(corr_x_ph, corr_y_ph);
#if 0
// to be restored when python code ported to c++
            df.lat_ph[i] = point.x;
            df.lon_ph[i] = point.y;
#else
            df.refracted_lat[i] = point.x;
            df.refracted_lon[i] = point.y;
#endif
        }
    }

    /* Mark Completion */
    updateRunTime(TimeLib::latchtime() - start);
    return true;
}
