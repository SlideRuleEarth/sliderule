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
 * geotiff_read
 *----------------------------------------------------------------------------*/
int geotiff_read (lua_State* L)
{
    bool status = false;

    /* Get Raster Data */
    const char* raster = LuaObject::getLuaString(L, 1);
    printf("DATA: %s\n", raster);


    TIFF* tif = TIFFClientOpen("Memory", "r", (thandle_t)raster,
                                NULL,   // tiff_Read
                                NULL,   // tiff_Write
                                NULL,   // tiff_Seek
                                NULL,   // tiff_Close
                                NULL,   // tiff_Size
                                NULL,   // tiff_Map
                                NULL);  // tiff_Unmap
    if (tif)
    {
        uint32_t imagelength;
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
        tmsize_t rowlength = TIFFScanlineSize(tif);
        tdata_t buf = _TIFFmalloc(rowlength);
        for(uint32_t row = 0; row < imagelength; row++)
        {
            TIFFReadScanline(tif, buf, row, 0);
            uint8_t* byte_ptr = (uint8_t*)buf;
            printf("[%ld] ", rowlength); for(int i = 0; i < rowlength; i++) printf("%02X ", byte_ptr[i]); printf("\n");
        }
        _TIFFfree(buf);
        TIFFClose(tif);
    }
    TIFFClose(tif);

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
        {"read",        geotiff_read},
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
