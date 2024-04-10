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
#include <proj.h>
#include "ogr_spatialref.h"

#include "GeoLib.h"
#include "LuaObject.h"

/******************************************************************************
 * LOCAL TYPES
 ******************************************************************************/

typedef struct {
    OGRSpatialReferenceH srs_in;
    OGRSpatialReferenceH srs_out;
    OGRCoordinateTransformationH transform;
} ogr_trans_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoLib::DEFAULT_CRS = "EPSG:7912";  // as opposed to "EPSG:4326"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * getUTMTransform
 *----------------------------------------------------------------------------*/
 GeoLib::utm_transform_t GeoLib::getUTMTransform (utm_zone_t utm, const char* input_crs)
 {
    ogr_trans_t* ogr_trans = new ogr_trans_t;
    ogr_trans->srs_in = OSRNewSpatialReference(NULL);
    ogr_trans->srs_out = OSRNewSpatialReference(NULL);
    OSRSetFromUserInput(ogr_trans->srs_in, input_crs);
    OSRSetProjCS(ogr_trans->srs_out, "UTM");
    OSRSetUTM(ogr_trans->srs_out, utm.zone, utm.is_north);
    ogr_trans->transform = OCTNewCoordinateTransformation(ogr_trans->srs_in, ogr_trans->srs_out);
    return reinterpret_cast<utm_transform_t>(ogr_trans);
 }

/*----------------------------------------------------------------------------
 * calcUTMCoordinates
 *----------------------------------------------------------------------------*/
 void GeoLib::freeUTMTransform (utm_transform_t transform)
 {
    ogr_trans_t* ogr_trans = reinterpret_cast<ogr_trans_t*>(transform);
    OSRDestroySpatialReference(ogr_trans->srs_in);
    OSRDestroySpatialReference(ogr_trans->srs_out);
    OCTDestroyCoordinateTransformation(ogr_trans->transform);
    delete ogr_trans;
}

/*----------------------------------------------------------------------------
 * calcUTMCoordinates
 *----------------------------------------------------------------------------*/
GeoLib::utm_zone_t GeoLib::calcUTMZone (double latitude, double longitude)
{
    utm_zone_t utm = {
        .zone = static_cast<int>(ceil((longitude + 180.0) / 6)),
        .is_north = (latitude >= 0.0)
    };
    return utm;
}

/*----------------------------------------------------------------------------
 * calcUTMCoordinates
 *----------------------------------------------------------------------------*/
bool GeoLib::calcUTMCoordinates(utm_transform_t transform, double latitude, double longitude, point_t& coord)
{
    bool status = false;

    /* Pull Out Transformation */
    ogr_trans_t* ogr_trans = reinterpret_cast<ogr_trans_t*>(transform);

    /* Perform Transformation 
     *  TODO: why is the x and y flipped?
     *        it only gives the correct answer when in this order */
    double x = latitude;
    double y = longitude;
    if(OCTTransform(ogr_trans->transform, 1, &x, &y, NULL) == TRUE) 
    {
        coord.x = x;
        coord.y = y;
        status = true;
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * luaCalcUTM
 *----------------------------------------------------------------------------*/
int GeoLib::luaCalcUTM (lua_State* L)
{
    double latitude;
    double longitude;

    try 
    {
        latitude = LuaObject::getLuaFloat(L, 1);
        longitude = LuaObject::getLuaFloat(L, 2);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to get parameters for UTM calculation: %s", e.what());
        return 0;
    }

    utm_zone_t utm = calcUTMZone(latitude, longitude);
    utm_transform_t transform = getUTMTransform(utm);

    int num_ret = 0;
    point_t coord;
    if(calcUTMCoordinates(transform, latitude, longitude, coord))
    {
        lua_pushinteger(L, utm.zone);
        lua_pushnumber(L, coord.x);
        lua_pushnumber(L, coord.y);
        num_ret = 3;
    }
    else
    {
        mlog(CRITICAL, "Failed to perform UTM transformation on %lf, %lf", latitude, longitude);
    }

    freeUTMTransform(transform);

    return num_ret;
}
