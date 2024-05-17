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
 * TIFFImage Subclass
 ******************************************************************************/

const char* GeoLib::TIFFImage::OBJECT_TYPE = "TIFFImage";
const char* GeoLib::TIFFImage::LUA_META_NAME = "TIFFImage";
const struct luaL_Reg GeoLib::TIFFImage::LUA_META_TABLE[] = {
    {"dimensions",  GeoLib::TIFFImage::luaDimensions},
    {"pixel",       GeoLib::TIFFImage::luaPixel},
    {"tobmp",       GeoLib::TIFFImage::luaConvertToBMP},
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaCreate (lua_State* L)
{
    try
    {
        const char* filename = getLuaString(L, 1);
        return createLuaObject(L, new TIFFImage(L, filename));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::TIFFImage(lua_State* L, const char* filename):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
    TIFF* tif = TIFFOpen(filename, "r");
    if(!tif) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to open tiff file: %s", filename);

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &length);
    mlog(INFO, "Reading image %s which is %u x %u pixels", filename, width, length);

    size = width * length;
    raster = new uint32_t[size];

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
    const uint32_t offset = (y * width) + x;
    if(offset < size) return raster[offset];
    return INVALID_PIXEL;
}

/*----------------------------------------------------------------------------
 * getWidth
 *----------------------------------------------------------------------------*/
uint32_t GeoLib::TIFFImage::getWidth(void) const
{
    return width;
}

/*----------------------------------------------------------------------------
 * getLength
 *----------------------------------------------------------------------------*/
uint32_t GeoLib::TIFFImage::getLength() const
{
    return length;
}

/*----------------------------------------------------------------------------
 * luaDimensions
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaDimensions (lua_State* L)
{
    try
    {
        GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        lua_pushnumber(L, lua_obj->width);
        lua_pushnumber(L, lua_obj->length);
        return returnLuaStatus(L, true, 3);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaPixel
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaPixel (lua_State* L)
{
    bool status = false;
    try
    {
        GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        const int x = getLuaInteger(L, 2);
        const int y = getLuaInteger(L, 3);
        if(x < 0 || x >= (int)lua_obj->width || y < 0 || y >= (int)lua_obj->length)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "out of bounds (%d, %d) ^ (%d, %d)", x, y, lua_obj->width, lua_obj->length);
        }
        lua_pushnumber(L, lua_obj->getPixel(x, y));
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "failed to get pixel: %s", e.what());
        lua_pushnil(L);
    }
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaConvertToBMP
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaConvertToBMP (lua_State* L)
{
    bool status = false;
    try
    {
        const GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        const char* bmp_filename = getLuaString(L, 2);
        status = GeoLib::writeBMP(lua_obj->raster, lua_obj->width, lua_obj->length, bmp_filename);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "failed to convert to BMP: %s", e.what());
    }
    return returnLuaStatus(L, status);
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/

void customTIFFErrorHandler(const char* , const char* , va_list ) // NOLINT [readability-named-parameter]
{
    // Ignore or suppress error messages here
}
void GeoLib::init (void)
{
    TIFFSetErrorHandler(NULL); // disables error messages
    TIFFSetWarningHandler(NULL); // disables warning messages
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

    UTMTransform transform(latitude, longitude);
    const point_t coord = transform.calculateCoordinates(latitude, longitude);
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

/*----------------------------------------------------------------------------
 * writeBMP
 *----------------------------------------------------------------------------*/
bool GeoLib::writeBMP (const uint32_t* data, int width, int height, const char* filename)
{
    /* open file */
    FILE* bmp_file = fopen(filename, "w");
    if(!bmp_file)
    {
        char errbuf[256];
        mlog(CRITICAL, "Failed to open file %s: %s", filename, strerror_r(errno, errbuf, sizeof(errbuf)));
        return false;
    }

    /* populate attributes */
    const uint32_t palette_size = 1024; // 2^(pixelsize) * 4
	const uint32_t image_size	= height * modup(width, 4);
	const uint32_t data_offset  = 0x36 + palette_size; // header plus palette
	const uint32_t file_size	= data_offset + image_size;

    /* populate header */
    bmp_hdr_t bmp_hdr = {
        .file_size          = file_size,
        .reserved1          = 0,
        .reserved2          = 0,
        .data_offset        = data_offset,
        .hdr_size           = 40,
        .image_width        = width,
        .image_height       = height,
        .color_planes       = 1,
        .color_depth        = 8,
        .compression        = 0,
        .image_size         = image_size,
        .hor_res            = 1,
        .ver_res            = 1,
        .palette_colors     = 0,
        .important_colors   = 0,
    };

    /* write header */
    fwrite("B", 1, 1, bmp_file); // magic numbers written first to avoid alignment issues
    fwrite("M", 1, 1, bmp_file); // "
    fwrite(&bmp_hdr, sizeof(bmp_hdr_t), 1, bmp_file);

    /* write color palette */
    for(int i = 0; i < 256; i++)
    {
        uint8_t b = static_cast<uint8_t>(i);
        union {
            uint8_t b[4];
            uint32_t v;
        } value = {
            .b = {b, b, b, b}
        };
        (void)value.b; // silence static analysis "unused variable"
        fwrite(&value.v, 4, 1, bmp_file);
    }

    /* write image data */
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            const int offset = (y * width) + x;
            const double normalized_pixel = (static_cast<double>(data[offset]) / static_cast<double>(0xFFFFFFFF)) * 256.0;
            uint8_t scaled_pixel = static_cast<uint8_t>(normalized_pixel);
            fwrite(&scaled_pixel, 1, 1, bmp_file);
        }
        for(int padding = 0; padding < modup(width, 4); padding++)
        {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, bmp_file);
        }
    }

    /* close file */
    fclose(bmp_file);

    /* return success */
    return true;
}