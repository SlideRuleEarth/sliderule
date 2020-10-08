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
#include "LuaObject.h"

/******************************************************************************
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    double lat0;    // southern
    double lon0;    // western
    double lat1;    // northern
    double lon1;    // eastern
} spatialspan_t;

class SpatialIndex: public AssetIndex<spatialspan_t>
{
    public:

                        SpatialIndex    (lua_State* L, Asset* _asset, int _threshold);
                        ~SpatialIndex   (void);

        static int      luaCreate       (lua_State* L);

        void            split           (node_t* node, spatialspan_t& lspan, spatialspan_t& rspan) override;
        bool            isleft          (node_t* node, const spatialspan_t& span) override;
        bool            isright         (node_t* node, const spatialspan_t& span) override;
        bool            intersect       (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   combine         (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   luatable2span   (lua_State* L, int parm) override;
        void            display         (const spatialspan_t& span) override;
    
    private:

        typedef enum {
            NORTH_POLAR,
            SOUTH_POLAR,
        } proj_t;

        typedef struct {            
            double  x;
            double  y;
        } coord_t;

        proj_t          classify        (spatialspan_t span);
        coord_t         project         (proj_t p, double lat, double lon);
};

#endif  /* __spatial_index__ */
