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

#ifndef __asset_index__
#define __asset_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "List.h"
#include "LuaObject.h"

/******************************************************************************
 * DEVICE OBJECT CLASS
 ******************************************************************************/

class AssetIndex: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate   (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int RESOURCE_NAME_MAX_LENGTH = 150;

        /*--------------------------------------------------------------------
         * TimeSpan Subclass
         *--------------------------------------------------------------------*/

        class TimeSpan
        {
            public:

                typedef struct {
                    List<int>           ril;   // resource index list
                    double              t[2];
                } node_t;
            
                        TimeSpan        (AssetIndex* _asset);
                        ~TimeSpan       (void);
                bool    add             (int ri); // resource index
                
            private:

                AssetIndex*             asset;
                node_t*                 root;
        };

        /*--------------------------------------------------------------------
         * SpatialRegion Subclass
         *--------------------------------------------------------------------*/

        class SpatialRegion
        {
            public:

                typedef struct {
                    List<int>           ril;    // resource index list
                    double              lat[2]; // southern, northern
                    double              lon[2]; // western, eastern
                } node_t;

                        SpatialRegion   (AssetIndex* _asset);
                        ~SpatialRegion  (void);
                bool    add             (int ri); // resource index
                
            private:

                AssetIndex*             asset;
                node_t*                 root;
        };

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            char                        name[RESOURCE_NAME_MAX_LENGTH];
            double                      t[2];   // start, stop
            double                      lat[2]; // southern, northern
            double                      lon[2]; // western, eastern
            Dictionary<double>          attr;   // attributes
        } resource_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Dictionary<AssetIndex*>  assets;
        static Mutex                    assetsMut;
        bool                            registered;

        const char*                     name;
        const char*                     format;
        const char*                     url;

        List<resource_t>                resources;
        TimeSpan                        timeIndex;
        SpatialRegion                   spatialIndex;


// parse index file and build all of the above indexes if the data is present in the index file (note that t0, t1, lat0, ... are all keywords)

// provide lua functions:
// range([list of time ranges]) --> list of objects
// polygon([list of lat,lon polygons]) --> list of objects

// select([list of field value expressions]) --> list of objects
// note that the expression should include NOT, AND, OR, ==, <, >, <=, >=

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        AssetIndex      (lua_State* L, const char* name, const char* format, const char* url);
        virtual         ~AssetIndex     (void);

        static int      luaInfo         (lua_State* L);
        static int      luaLoad         (lua_State* L);
};

#endif  /* __asset_index__ */
