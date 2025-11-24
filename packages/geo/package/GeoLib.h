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

#ifndef __geo_lib__
#define __geo_lib__

#include "OsApi.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "RegionMask.h"
#include "MathLib.h"

class GeoLib: public MathLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* DEFAULT_CRS;

        /*--------------------------------------------------------------------
         * UTMTransform Subclass
         *--------------------------------------------------------------------*/

        class UTMTransform
        {
            public:
                UTMTransform(double initial_latitude, double initial_longitude, const char* input_crs=DEFAULT_CRS);
                UTMTransform(int _zone, bool _is_north, const char* output_crs=DEFAULT_CRS);
                ~UTMTransform(void);
                point_t calculateCoordinates(double x, double y);
                int zone;
                bool is_north;
                bool in_error;
            private:
                typedef void* utm_transform_t;
                utm_transform_t transform;
        };

        /*--------------------------------------------------------------------
         * TIFFImage Subclass
         *--------------------------------------------------------------------*/

        class TIFFImage: public LuaObject
        {
            static const int RASTER_DATA_ALIGNMENT = 8;

            public:
                static const char* OBJECT_TYPE;
                static const char* LUA_META_NAME;
                static const struct luaL_Reg LUA_META_TABLE[];
                static const uint64_t INVALID_PIXEL = 0xFFFFFFFFFFFFFFFFL;
                static const long LIBTIFF_DRIVER = 0;
                static const long GDAL_DRIVER = 1;
                static int luaCreate (lua_State* L);
                typedef union {
                    double f64;
                    float f32;
                    uint64_t u64;
                    uint32_t u32;
                    uint16_t u16;
                    uint8_t u8;
                    int64_t i64;
                    int32_t i32;
                    int16_t i16;
                    int8_t i8;
                } val_t;
                TIFFImage (lua_State* L, const char* filename, long driver=LIBTIFF_DRIVER);
                ~TIFFImage (void) override;
                val_t getPixel (uint32_t x, uint32_t y);
                uint32_t getWidth (void) const;
                uint32_t getHeight (void) const;
            private:
                static int luaDimensions (lua_State* L);
                static int luaPixel (lua_State* L);
                static int luaConvertToBMP (lua_State* L);
                uint32_t width;
                uint32_t height;
                uint32_t typesize;
                uint32_t size;
                uint8_t* raster;
                RecordObject::fieldType_t type;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void init (void);
        static int luaCalcUTM (lua_State* L);
        static int luaPolySimplify (lua_State* L);
        static bool writeBMP (const uint32_t* data, int width, int height, const char* filename, uint32_t min_val=0, uint32_t max_val=0xFFFFFFFF);
        static bool burnGeoJson (RegionMask& image);
        
    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t file_size;         // total file size
            uint16_t reserved1;         // application dependent
            uint16_t reserved2;         // application dependent
            uint32_t data_offset;       // start of image data after DIB header
            uint32_t hdr_size;          // must be 40 - start of DIB v3 header
            int32_t  image_width;       // signed
            int32_t  image_height;      // signed
            uint16_t color_planes;      // must be 1
            uint16_t color_depth;       // bits per pixel
            uint32_t compression;       // 0 - none, 1 - rle 8 bits, 2 - rle 4 bits, 3 - bit field 16/32 bits, 4 - jpeg, 5 - png
            uint32_t image_size;        // only image, not file
            uint32_t hor_res;           // horizontal pixels per meter
            uint32_t ver_res;           // vertical pixels per meter
            uint32_t palette_colors;    // 0 defaults to 2^n
            uint32_t important_colors;  // 0 defaults to all
        } bmp_hdr_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int modup (int val, int mod) { return (mod - (val % mod)) % mod; }

};

#endif /* __geo_lib__ */
