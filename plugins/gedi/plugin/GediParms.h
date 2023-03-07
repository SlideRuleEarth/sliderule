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

#ifndef __gedi_parms__
#define __gedi_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "GeoJsonRaster.h"
#include "List.h"
#include "MathLib.h"

/******************************************************************************
 * GEDI PARAMETERS
 ******************************************************************************/

class GediParms: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Beams */
        typedef enum {
            ALL_BEAMS = -1,
            BEAM0000 = 0,
            BEAM0001 = 1,
            BEAM0010 = 2,
            BEAM0011 = 3,
            BEAM0101 = 5,
            BEAM0110 = 6,
            BEAM1000 = 8,
            BEAM1011 = 11,
            NUM_BEAMS = 8
        } beam_t;

        /* Degrade Flag */
        typedef enum {
            DEGRADE_UNFILTERED = -1,
            DEGRADE_UNSET = 0,
            DEGRADE_SET = 1
        } degrade_t;

        /* L2 Quality Flag */
        typedef enum {
            L2QLTY_UNFILTERED = -1,
            L2QLTY_UNSET = 0,
            L2QLTY_SET = 1
        } l2_quality_t;

        /* L4 Quality Flag */
        typedef enum {
            L4QLTY_UNFILTERED = -1,
            L4QLTY_UNSET = 0,
            L4QLTY_SET = 1
        } l4_quality_t;

        /* SURFACE Flag */
        typedef enum {
            SURFACE_UNFILTERED = -1,
            SURFACE_UNSET = 0,
            SURFACE_SET = 1
        } surface_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* POLYGON;
        static const char* RASTER;
        static const char* LATITUDE;
        static const char* LONGITUDE;
        static const char* DEGRADE_FLAG;
        static const char* L2_QUALITY_FLAG;
        static const char* L4_QUALITY_FLAG;
        static const char* SURFACE_FLAG;
        static const char* BEAM;
        static const char* RQST_TIMEOUT;
        static const char* NODE_TIMEOUT;
        static const char* READ_TIMEOUT;
        static const char* GLOBAL_TIMEOUT; // sets all timeouts at once

        static const int DEFAULT_RQST_TIMEOUT       = 600; // seconds
        static const int DEFAULT_NODE_TIMEOUT       = 600; // seconds
        static const int DEFAULT_READ_TIMEOUT       = 600; // seconds

        static const int64_t GEDI_SDP_EPOCH_GPS     = 1198800018; // seconds to add to GEDI delta times to get GPS times

        static const uint8_t BEAM_NUMBER[NUM_BEAMS];

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        static const char*  beam2group          (int beam);
        static int64_t      deltatime2timestamp (double delta_time);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<MathLib::coord_t>  polygon;                        // polygon of region of interest
        GeoJsonRaster*          raster;                         // raster of region of interest, created from geojson file
        int                     beam;                           // beam number or -1 for all
        degrade_t               degrade_filter;
        l2_quality_t            l2_quality_filter;
        l4_quality_t            l4_quality_filter;
        surface_t               surface_filter;
        int                     rqst_timeout;                   // total time in seconds for request to be processed
        int                     node_timeout;                   // time in seconds for a single node to work on a distributed request (used for proxied requests)
        int                     read_timeout;                   // time in seconds for a single read of an asset to take

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                GediParms               (lua_State* L, int index);
                                ~GediParms              (void);

        void                    cleanup                 (void);
        void                    get_lua_polygon         (lua_State* L, int index, bool* provided);
        void                    get_lua_raster          (lua_State* L, int index, bool* provided);
};

#endif  /* __gedi_parms__ */
