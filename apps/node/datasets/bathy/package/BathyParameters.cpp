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
 * INCLUDE
 ******************************************************************************/

#include "geo.h"
#include "OsApi.h"
#include "BathyParameters.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyParameters::LUA_META_NAME = "BathyParameters";

const double BathyParameters::NIGHT_SOLAR_ELEVATION_THRESHOLD = 5.0; // degrees
const double BathyParameters::MINIMUM_HORIZONTAL_SUBAQUEOUS_UNCERTAINTY = 0.0; // meters
const double BathyParameters::MINIMUM_VERTICAL_SUBAQUEOUS_UNCERTAINTY = 0.10; // meters
const double BathyParameters::DEFAULT_WIND_SPEED = 3.3; // meters

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void BathyParameters::fromLua (lua_State* L, int index)
{
    Atl03Parameters::fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyParameters::BathyParameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    Atl03Parameters (L, key_space, asset_name, _resource, lua_meta_name)
{
    addParameter("asset09",             &atl09AssetName,      "Asset identifier to use when reading wind speed from ATL09; when left blank the server will do the right thing");
    addParameter("max_dem_delta",       &maxDemDelta,         "Maximum positive vertical distance of photon from ATL03 DEM to include in bathymetry processing");
    addParameter("min_dem_delta",       &minDemDelta,         "Maximum negative vertical distance of photon from ATL03 DEM to include in bathymetry processing");
    addParameter("max_geoid_delta",     &maxGeoidDelta,       "Maximum positive vertical distance of photon from ATL03 geoid (sea level) to include in bathymetry processing");
    addParameter("min_geoid_delta",     &minGeoidDelta,       "Maximum negative vertical distance of photon from ATL03 geoid (sea level) to include in bathymetry processing");
    addParameter("ph_in_extent",        &phInExtent,          "Hard coded number of photons to include in the variable length segment; overrides 'len' and 'res' parameters");
    addParameter("generate_ndwi",       &generateNdwi,        "Boolean indicating that the server should calculate and include an NDWI value for each photon");
    addParameter("use_bathy_mask",      &useBathyMask,        "Boolean controlling the use of the global bathymetry mask; if enabled all input granules will be subsetted to the bathymetry mask prior to any other subsetting");
    addParameter("surface",             &surface,             "Configuration structure that controls the calculation and inclusion of the sea surface height at each photon");
    addParameter("refraction",          &refraction,          "Configuration structure that controls performing refraction correction on the subaqueous bathymetry photons");
    addParameter("uncertainty",         &uncertainty,         "Configuration structure that controls performing and including an uncertainty calculation for each bathymetry photon");
}
