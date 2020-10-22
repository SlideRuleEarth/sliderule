/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __spatial_index__
#define __spatial_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "AssetIndex.h"
#include "MathLib.h"
#include "LuaObject.h"

/******************************************************************************
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    MathLib::coord_t c0;
    MathLib::coord_t c1;
} spatialspan_t;

class SpatialIndex: public AssetIndex<spatialspan_t>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        SpatialIndex    (lua_State* L, Asset* _asset, MathLib::proj_t _projection, int _threshold);
                        ~SpatialIndex   (void);

        static int      luaCreate       (lua_State* L);

        void            split           (node_t* node, spatialspan_t& lspan, spatialspan_t& rspan) override;
        bool            isleft          (node_t* node, const spatialspan_t& span) override;
        bool            isright         (node_t* node, const spatialspan_t& span) override;
        bool            intersect       (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   combine         (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   attr2span       (Dictionary<double>* attr, bool* provided=NULL) override;
        spatialspan_t   luatable2span   (lua_State* L, int parm) override;
        void            displayspan     (const spatialspan_t& span) override;
    
    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            MathLib::point_t p0;
            MathLib::point_t p1;
        } polarspan_t;    

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        polarspan_t     project         (spatialspan_t span);
        spatialspan_t   restore         (polarspan_t polar);

        static int      luaPolar        (lua_State* L);
        static int      luaSphere       (lua_State* L);
        static int      luaSplit        (lua_State* L);
        static int      luaIntersect    (lua_State* L);
        static int      luaCombine      (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        MathLib::proj_t projection;
};

#endif  /* __spatial_index__ */
