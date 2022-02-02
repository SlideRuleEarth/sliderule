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

#include "GeoTIFFFile.h"
#include <tiffio.h>
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoTIFFFile::LuaMetaName = "GeoTIFFFile";
const struct luaL_Reg GeoTIFFFile::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"pixel",       luaPixel},
    {NULL,          NULL}
};

const char* GeoTIFFFile::IMAGE_KEY = "image";
const char* GeoTIFFFile::IMAGELENGTH_KEY = "imagelength";
const char* GeoTIFFFile::DIMENSION_KEY = "dimension";
const char* GeoTIFFFile::BBOX_KEY = "bbox";
const char* GeoTIFFFile::CELLSIZE_KEY = "cellsize";


/******************************************************************************
 * FILE STATIC LIBTIFF METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * libtiff call-back parameter
 *----------------------------------------------------------------------------*/
typedef struct {
    const uint8_t* filebuf;
    long filesize;
    long pos;
} geotiff_cb_t;

/*----------------------------------------------------------------------------
 * libtiff_read
 *----------------------------------------------------------------------------*/
static tsize_t libtiff_read(thandle_t st, tdata_t buffer, tsize_t size)
{
    geotiff_cb_t* g = (geotiff_cb_t*)st;
    tsize_t bytes_left = g->filesize - g->pos;
    tsize_t bytes_to_read = MIN(bytes_left, size);
    LocalLib::copy(buffer, &g->filebuf[g->pos], bytes_to_read);
    g->pos += bytes_to_read;
    return bytes_to_read;
};

/*----------------------------------------------------------------------------
 * libtiff_write
 *----------------------------------------------------------------------------*/
static tsize_t libtiff_write(thandle_t, tdata_t, tsize_t)
{
    return 0;
};

/*----------------------------------------------------------------------------
 * libtiff_seek
 *----------------------------------------------------------------------------*/
static toff_t libtiff_seek(thandle_t st, toff_t pos, int whence)
{
    geotiff_cb_t* g = (geotiff_cb_t*)st;
    if(whence == SEEK_SET)      g->pos = pos;
    else if(whence == SEEK_CUR) g->pos += pos;
    else if(whence == SEEK_END) g->pos = g->filesize + pos;
    return g->pos;
};

/*----------------------------------------------------------------------------
 * libtiff_close
 *----------------------------------------------------------------------------*/
static int libtiff_close(thandle_t)
{
    return 0;
};

/*----------------------------------------------------------------------------
 * libtiff_size
 *----------------------------------------------------------------------------*/
static toff_t libtiff_size(thandle_t st)
{
    geotiff_cb_t* g = (geotiff_cb_t*)st;
    return g->filesize;
};

/*----------------------------------------------------------------------------
 * libtiff_map
 *----------------------------------------------------------------------------*/
static int libtiff_map(thandle_t st, tdata_t* addr, toff_t* pos)
{
    geotiff_cb_t* g = (geotiff_cb_t*)st;
    *pos = g->pos;
    *addr = (void*)&g->filebuf[g->pos];
    return 0;
};

/*----------------------------------------------------------------------------
 * libtiff_unmap
 *----------------------------------------------------------------------------*/
static void libtiff_unmap(thandle_t, tdata_t, toff_t)
{
    return;
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      image=<image>,
 *      imagelength=<imagelength>,
 *      [bbox=<<lon_min>, <lat_min>, <lon_max>, <lat_max>>,
 *      cellsize=<cell size>]
 *  })
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoTIFFFile* GeoTIFFFile::create (lua_State* L, int index)
{
    bbox_t _bbox = {0.0, 0.0, 0.0, 0.0 };

    /* Get Image */
    lua_getfield(L, index, IMAGE_KEY);
    const char* image = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get Image Length */
    lua_getfield(L, index, IMAGELENGTH_KEY);
    long imagelength = getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Optionally Get Bounding Box */
    lua_getfield(L, index, BBOX_KEY);
    if(lua_istable(L, -1) && (lua_rawlen(L, -1) == 4))
    {
        lua_rawgeti(L, -1, 1);
        _bbox.lon_min = getLuaFloat(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, -1, 2);
        _bbox.lat_min = getLuaFloat(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, -1, 3);
        _bbox.lon_max = getLuaFloat(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, -1, 4);
        _bbox.lat_max = getLuaFloat(L, -1);
        lua_pop(L, 1);
    }

    /* Optionally Get Cell Size */
    lua_getfield(L, index, CELLSIZE_KEY);
    double _cellsize = getLuaFloat(L, -1, true, 0.0);
    lua_pop(L, 1);

    /* Create GeoTIFF File */
    return new GeoTIFFFile(L, image, imagelength, _bbox, _cellsize);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoTIFFFile::~GeoTIFFFile(void)
{
    if(raster) delete [] raster;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoTIFFFile::GeoTIFFFile(lua_State* L, const char* image, long imagelength, bbox_t _bbox, double _cellsize):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(image);

    /* Initialize Class Data Members */
    rows = 0;
    cols = 0;
    raster = NULL;
    bbox = _bbox;
    cellsize = _cellsize;

    /* Create LibTIFF Callback Data Structure */
    geotiff_cb_t g = {
        .filebuf = (const uint8_t*)image,
        .filesize = imagelength,
        .pos = 0
    };

    /* Open TIFF via Memory Callbacks */
    TIFF* tif = TIFFClientOpen("Memory", "r", (thandle_t)&g,
                                libtiff_read,
                                libtiff_write,
                                libtiff_seek,
                                libtiff_close,
                                libtiff_size,
                                libtiff_map,
                                libtiff_unmap);

    /* Read TIFF */
    if(tif)
    {
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &cols);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &rows);

        /* Create User Data */
        uint32_t strip_size = TIFFStripSize(tif);
        uint32_t num_strips = TIFFNumberOfStrips(tif);
        long size = strip_size * num_strips;
        if(size > 0 && size < GEOTIFF_MAX_IMAGE_SIZE)
        {
            /* Allocate Image */
            raster = new uint8_t [size];

            /* Read Data */
            for(uint32_t strip = 0; strip < num_strips; strip++)
            {
                TIFFReadEncodedStrip(tif, strip, &raster[strip * strip_size], (tsize_t)-1);
            }

            /* Clean Up */
            TIFFClose(tif);
        }
        else
        {
            mlog(CRITICAL, "Invalid image size: %ld\n", size);
        }
    }
    else
    {
        mlog(CRITICAL, "Unable to open memory mapped tiff file");
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaDimensions (lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoTIFFFile* lua_obj = (GeoTIFFFile*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->rows);
        lua_pushinteger(L, lua_obj->cols);
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting dimensions: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaBoundingBox - :bbox() --> (lon_min, lat_min, lon_max, lat_max)
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaBoundingBox(lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoTIFFFile* lua_obj = (GeoTIFFFile*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->bbox.lon_min);
        lua_pushinteger(L, lua_obj->bbox.lat_min);
        lua_pushinteger(L, lua_obj->bbox.lon_max);
        lua_pushinteger(L, lua_obj->bbox.lat_max);
        num_ret += 4;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting bounding box: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaCellSize - :cell() --> cell size
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaCellSize(lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoTIFFFile* lua_obj = (GeoTIFFFile*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->cellsize);
        num_ret += 1;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting cell size: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaPixel - :pixel(r, c) --> on|off
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaPixel (lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoTIFFFile* lua_obj = (GeoTIFFFile*)getLuaSelf(L, 1);

        /* Get Pixel Index */
        uint32_t r = getLuaInteger(L, 2);
        uint32_t c = getLuaInteger(L, 3);

        /* Get Pixel */
        if((r < lua_obj->rows) && (c < lua_obj->cols))
        {
            lua_pushboolean(L, lua_obj->rawPixel(r,c));
            num_ret++;

            /* Set Return Status */
            status = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, "invalid index provided <%d, %d>", r, c);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting pixel: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaSubset - :subset(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int GeoTIFFFile::luaSubset (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        GeoTIFFFile* lua_obj = (GeoTIFFFile*)getLuaSelf(L, 1);

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Inclusion */
        status = lua_obj->subset(lon, lat);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error subsetting: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
