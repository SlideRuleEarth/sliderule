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

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * geotiff_ client call-backs
 *----------------------------------------------------------------------------*/
typedef struct {
    const uint8_t* filebuf;
    long filesize;
    long pos;
} geotiff_t;

static tsize_t geotiff_Read(thandle_t st, tdata_t buffer, tsize_t size)
{
    geotiff_t* g = (geotiff_t*)st;
    tsize_t bytes_left = g->filesize - g->pos;
    tsize_t bytes_to_read = MIN(bytes_left, size);
    LocalLib::copy(buffer, &g->filebuf[g->pos], bytes_to_read);
    return bytes_to_read;
};

static tsize_t geotiff_Write(thandle_t, tdata_t, tsize_t)
{
    return 0;
};

static toff_t geotiff_Seek(thandle_t st, toff_t pos, int whence)
{
    geotiff_t* g = (geotiff_t*)st;
    if(whence == SEEK_SET)      g->pos = pos;
    else if(whence == SEEK_CUR) g->pos += pos;
    else if(whence == SEEK_END) g->pos = g->filesize + pos;
    return g->pos;
};

static int geotiff_Close(thandle_t)
{
    return 0;
};

static toff_t geotiff_Size(thandle_t st)
{
    geotiff_t* g = (geotiff_t*)st;
    return g->filesize;
};

static int geotiff_Map(thandle_t, tdata_t*, toff_t*)
{
    return 0;
};

static void geotiff_Unmap(thandle_t, tdata_t, toff_t)
{
    return;
};

/*----------------------------------------------------------------------------
 * geotiff_scanline
 *----------------------------------------------------------------------------*/
int geotiff_scanline (lua_State* L)
{
    bool status = false;

    /* Get Raster Data */
    const char* raster = LuaObject::getLuaString(L, 1);
    long imagelength = LuaObject::getLuaInteger(L, 2);

    printf("DATA[%ld]: ", imagelength);
    uint8_t* bptr = (uint8_t*)raster;
    for(int i = 0; i < imagelength; i++) printf("%02X ", bptr[i]); printf("\n");

    geotiff_t g = {
        .filebuf = (const uint8_t*)raster,
        .filesize = imagelength,
        .pos = 0
    };

//    TIFF* tif = TIFFOpen("map.tiff", "r");
    TIFF* tif = TIFFClientOpen("Memory", "r", (thandle_t)&g,
                                geotiff_Read,
                                geotiff_Write,
                                geotiff_Seek,
                                geotiff_Close,
                                geotiff_Size,
                                geotiff_Map,
                                geotiff_Unmap);
    if(tif)
    {
        tdata_t buf;
        tstrip_t strip;
        tmsize_t size = TIFFStripSize(tif);
        buf = _TIFFmalloc(size);
        for (strip = 0; strip < TIFFNumberOfStrips(tif); strip++)
        {
            TIFFReadEncodedStrip(tif, strip, buf, (tsize_t) -1);
            uint8_t* byteptr = (uint8_t*)buf;
            printf("[%ld]: ", size); for(int i = 0; i < size; i++) printf("%02X ", byteptr[i]); printf("\n");
        }

        _TIFFfree(buf);
        TIFFClose(tif);

        status = true;
    }

    /* Return Status */
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * geotiff_open
 *----------------------------------------------------------------------------*/
int geotiff_open (lua_State* L)
{
    static const struct luaL_Reg geotiff_functions[] = {
        {"scan",        geotiff_scanline},
        {NULL,          NULL}
    };

    /* Set Library */
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
