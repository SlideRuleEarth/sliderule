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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "geotiff.h"
#include <tiffio.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEOTIFF_LIBNAME  "geotiff"
const char* LUA_GEOTIFF_METANAME  = "geotiff.raster";

/******************************************************************************
 * LIBTIFF CALLBACKS
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
 * GEOTIFF LUA FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lua_scanline
 *----------------------------------------------------------------------------*/
static int lua_scanline (lua_State* L)
{
    /* Get Raster Data */
    const char* raster = LuaObject::getLuaString(L, 1);
    long imagelength = LuaObject::getLuaInteger(L, 2);

    /* Create LibTIFF Callback Data Structure */
    geotiff_cb_t g = {
        .filebuf = (const uint8_t*)raster,
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
        uint32_t cols, rows;
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &cols);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &rows);

        /* Create User Data */
        uint32_t strip_size = TIFFStripSize(tif);
        uint32_t num_strips = TIFFNumberOfStrips(tif);
        long size = strip_size * num_strips;
        if(size > 0 && size < GEOTIFF_MAX_IMAGE_SIZE)
        {
            /* Allocate Image */
            geotiff_t* geotiff_data = (geotiff_t*)lua_newuserdata(L, sizeof(geotiff_t) + size);
            geotiff_data->cols = cols;
            geotiff_data->rows = rows;

            /* Associate Metatable with Userdata */
            luaL_getmetatable(L, LUA_GEOTIFF_METANAME);
            lua_setmetatable(L, -2);

            /* Read Data */
            for(uint32_t strip = 0; strip < num_strips; strip++)
            {
                TIFFReadEncodedStrip(tif, strip, &geotiff_data->image[strip * strip_size], (tsize_t)-1);
            }

            /* Clean Up */
            TIFFClose(tif);
        }
        else
        {
            mlog(CRITICAL, "Invalid raster image size: %ld\n", size);
            lua_pushnil(L);
        }
    }
    else
    {
        mlog(CRITICAL, "Unable to open memory mapped tiff file");
        lua_pushnil(L);
    }

    /* Return GeoTIFF */
    return 1;
}

/*----------------------------------------------------------------------------
 * lua_dimensions - geotiff:dim() --> rows, cols
 *----------------------------------------------------------------------------*/
static int lua_dimensions (lua_State* L)
{
    geotiff_t* geotiff_data = (geotiff_t*)luaL_checkudata(L, 1, LUA_GEOTIFF_METANAME);
    if(geotiff_data)
    {
        lua_pushinteger(L, geotiff_data->rows);
        lua_pushinteger(L, geotiff_data->cols);
        return 2;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 * lua_pixel - geotiff:pix(r, c) --> on|off
 *----------------------------------------------------------------------------*/
static int lua_pixel (lua_State* L)
{
    geotiff_t* geotiff_data = (geotiff_t*)luaL_checkudata(L, 1, LUA_GEOTIFF_METANAME);
    uint32_t r = lua_tointeger(L, 2);
    uint32_t c = lua_tointeger(L, 3);

    if( geotiff_data && (r < geotiff_data->rows) && (c < geotiff_data->cols) )
    {
        lua_pushboolean(L, geotiff_data->image[(r * geotiff_data->cols) + c] == GEOTIFF_PIXEL_ON);
    }
    else
    {
        lua_pushboolean(L, false);
    }

    return 1;
}

/******************************************************************************
 * GEOTIFF FUNCTIONS
 ******************************************************************************/
/*----------------------------------------------------------------------------
 * geotiff metatable functions
 *----------------------------------------------------------------------------*/
const struct luaL_Reg geotiffLibsM [] = {
    {"dim",         lua_dimensions},
    {"pixel",       lua_pixel},
    {NULL, NULL}
};

/*----------------------------------------------------------------------------
 * geotiff_open
 *----------------------------------------------------------------------------*/
int geotiff_open (lua_State* L)
{
    static const struct luaL_Reg geotiff_functions[] = {
        {"scan",        lua_scanline},
        {NULL,          NULL}
    };

    /* Set GeoTIFF Library */
    luaL_newmetatable(L, LUA_GEOTIFF_METANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, geotiffLibsM, 0);

    /* Set Package Library */
    luaL_newlib(L, geotiff_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgeotiff (void)
{
    /* Extend Lua */
    LuaEngine::extend(LUA_GEOTIFF_LIBNAME, geotiff_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEOTIFF_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEOTIFF_LIBNAME, LIBID);
}

void deinitgeotiff (void)
{
}
}
