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

#ifndef __netsvc_parms__
#define __netsvc_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "List.h"
#include "MathLib.h"

#ifdef __geo__
#include "GeoJsonRaster.h"
#endif

/******************************************************************************
 * REQUEST PARAMETERS
 ******************************************************************************/

class RequestParms: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* SELF;
        static const char* POLYGON;
        static const char* RASTER;
        static const char* LATITUDE;
        static const char* LONGITUDE;
        static const char* PROJECTION;
        static const char* RQST_TIMEOUT;
        static const char* NODE_TIMEOUT;
        static const char* READ_TIMEOUT;
        static const char* GLOBAL_TIMEOUT; // sets all timeouts at once
        static const char* CLUSTER_SIZE_HINT;

        static const int DEFAULT_RQST_TIMEOUT       = 600; // seconds
        static const int DEFAULT_NODE_TIMEOUT       = 600; // seconds
        static const int DEFAULT_READ_TIMEOUT       = 600; // seconds

        static const int DEFAULT_CLUSTER_SIZE_HINT  = 0; // dynamic

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct RasterImpl
        {
#ifdef __geo__
            GeoJsonRaster*  _raster;
#else
            uint8_t*        _raster;
#endif

           RasterImpl   (void): _raster(NULL) {}
           ~RasterImpl  (void) {delete _raster;}
           bool valid   (void) const {return _raster != NULL;}

           /* Cannot be inlined here, must be implemented in cpp file */
           bool includes(double lon, double lat) const;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        const char*         tojson              (void) const override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<MathLib::coord_t>  polygon;                        // polygon of region of interest
        RasterImpl              raster;                         // raster of region of interest, created from geojson file
        int                     rqst_timeout;                   // total time in seconds for request to be processed
        int                     node_timeout;                   // time in seconds for a single node to work on a distributed request (used for proxied requests)
        int                     read_timeout;                   // time in seconds for a single read of an asset to take
        int                     cluster_size_hint;              // hint for the number of nodes in the cluster sliderule is running in
        MathLib::proj_t         projection;                     // which projection to use for the polygon
        MathLib::point_t*       projected_poly;                 // array of points that hold the projected polygon
        int                     points_in_poly;                 // number of points in the projected polygon

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                RequestParms            (lua_State* L, int index);
                                ~RequestParms           (void) override;
        void                    cleanup                 (void) const;
        void                    get_lua_polygon         (lua_State* L, int index, bool* provided);
        void                    get_lua_raster          (lua_State* L, int index, bool* provided);
        void                    get_lua_projection      (lua_State* L, int index, bool* provided);
};

#endif  /* __netsvc_parms__ */
