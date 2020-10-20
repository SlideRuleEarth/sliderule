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

#ifndef __asset__
#define __asset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "List.h"
#include "LuaObject.h"

/******************************************************************************
 * ASSET INDEX CLASS
 ******************************************************************************/

class Asset: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   RESOURCE_NAME_LENGTH = 150;
        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            char                            name[RESOURCE_NAME_LENGTH];
            Dictionary<double>              attributes{16};
        } resource_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        virtual     ~Asset      (void);

        int         load        (resource_t& resource);
        resource_t& operator[]  (int i);

        int         size        (void);
        const char* getName     (void);
        const char* getFormat   (void);
        const char* getUrl      (void);
        const char* getIndex    (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*                  LuaMetaName;
        static const struct luaL_Reg        LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Dictionary<Asset*>           assets;
        static Mutex                        assetsMut;
        bool                                registered;

        const char*                         name;
        const char*                         format;
        const char*                         url;
        const char*                         index;

        List<resource_t>                    resources;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Asset       (lua_State* L, const char* _name, const char* _format, const char* _url, const char* _index);

        static int      luaInfo     (lua_State* L);
        static int      luaLoad     (lua_State* L);
};

#endif  /* __asset__ */
