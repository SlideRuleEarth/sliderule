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

#ifndef __bathy_viewer__
#define __bathy_viewer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "List.h"
#include "LuaObject.h"
#include "OsApi.h"
#include "StringLib.h"
#include "RasterObject.h"
#include "H5Array.h"
#include "GeoLib.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * ATL03 TABLE BUILDER
 ******************************************************************************/

class BathyViewer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* GLOBAL_BATHYMETRY_MASK_FILE_PATH;
        static const double GLOBAL_BATHYMETRY_MASK_MAX_LAT;
        static const double GLOBAL_BATHYMETRY_MASK_MIN_LAT;
        static const double GLOBAL_BATHYMETRY_MASK_MAX_LON;
        static const double GLOBAL_BATHYMETRY_MASK_MIN_LON;
        static const double GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
        static const uint32_t GLOBAL_BATHYMETRY_MASK_OFF_VALUE;

        static const int32_t MAX_PH_IN_SEG = 100000;
        static const int32_t MIN_PH_IN_SEG = 0;

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/


        typedef struct Info {
            BathyViewer*   reader;
            char           prefix[7];
            int            track;
            int            pair;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                explicit Region     (info_t* info);
                ~Region             (void) = default;

                H5Array<double>     segment_lat;
                H5Array<double>     segment_lon;
                H5Array<int32_t>    segment_ph_cnt;
        };


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[Icesat2Fields::NUM_SPOTS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        const int           read_timeout_ms;
        Icesat2Fields*      parms;

        H5Coro::Context*    context; // for ATL03 file

        GeoLib::TIFFImage*  bathyMask;

        int64_t             totalPhotons;
        int64_t             totalPhotonsInMask;
        int64_t             totalSegments;
        int64_t             totalSegmentsInMask;
        int64_t             totalErrors;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            BathyViewer            (lua_State* L, Icesat2Fields* _parms);
                            ~BathyViewer           (void) override;

        static void*        subsettingThread        (void* parm);
        static int          luaCounts               (lua_State* L);
};

#endif  /* __bathy_viewer__ */
