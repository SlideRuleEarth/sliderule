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
#include <tiffio.h>
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
 * UTMTransform Subclass
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::UTMTransform::UTMTransform(double initial_latitude, double initial_longitude, const char* input_crs)
{
    zone = static_cast<int>(ceil((initial_longitude + 180.0) / 6)); // set public member
    is_north = (initial_latitude >= 0.0); // set public member
    in_error = false; // set public member
    ogr_trans_t* ogr_trans = new ogr_trans_t;
    ogr_trans->srs_in = OSRNewSpatialReference(NULL);
    ogr_trans->srs_out = OSRNewSpatialReference(NULL);
    OSRSetFromUserInput(ogr_trans->srs_in, input_crs);
    OSRSetProjCS(ogr_trans->srs_out, "UTM");
    OSRSetUTM(ogr_trans->srs_out, zone, is_north);
    ogr_trans->transform = OCTNewCoordinateTransformation(ogr_trans->srs_in, ogr_trans->srs_out);
    transform = reinterpret_cast<utm_transform_t>(ogr_trans); // set private member
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoLib::UTMTransform::~UTMTransform(void)
{
    ogr_trans_t* ogr_trans = reinterpret_cast<ogr_trans_t*>(transform);
    OSRDestroySpatialReference(ogr_trans->srs_in);
    OSRDestroySpatialReference(ogr_trans->srs_out);
    OCTDestroyCoordinateTransformation(ogr_trans->transform);
    delete ogr_trans;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoLib::point_t GeoLib::UTMTransform::calculateCoordinates(double latitude, double longitude)
{
    point_t coord;

    /* Force In Error To Be Set On Success */
    in_error = true;

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
        in_error = false;
    }

    /* Return Coordinates */
    return coord;
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

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

    UTMTransform transform(latitude, longitude);
    point_t coord = transform.calculateCoordinates(latitude, longitude);
    if(!transform.in_error)
    {
        lua_pushinteger(L, transform.zone);
        lua_pushnumber(L, coord.x);
        lua_pushnumber(L, coord.y);
        return 3;
    }
    else
    {
        mlog(CRITICAL, "Failed to perform UTM transformation on %lf, %lf", latitude, longitude);
        return 0;
    }
}

/******************************************************************************
 * TIFFImage Subclass
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::TIFFImage(const char* filename)
{
    TIFF* tif = TIFFOpen(filename, "r");
    if(!tif) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to open tiff file: %s", filename);

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &length);
    mlog(CRITICAL, "Reading image %s which is %u` x %u pixels", filename, width, length);

    raster = new uint32_t[width * length];

    if(!TIFFReadRGBAImage(tif, width, length, raster, 0)) 
    {
        delete [] raster;
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read tiff file: %s", filename);
    }

    TIFFClose(tif);    
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::~TIFFImage(void)
{
    delete [] raster;
}

/*----------------------------------------------------------------------------
 * getPixel
 *----------------------------------------------------------------------------*/
uint32_t GeoLib::TIFFImage::getPixel(uint32_t x, uint32_t y)
{
    return raster[(y * width) + x];
}
