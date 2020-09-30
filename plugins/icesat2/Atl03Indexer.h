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

#ifndef __atl03_indexer__
#define __atl03_indexer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "Asset.h"
#include "OsApi.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl03Indexer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            char    name[Asset::RESOURCE_NAME_LENGTH];
            double  t0;
            double  t1;
            double  lat0;
            double  lon0;
            double  lat1;
            double  lon1;
            int     cycle;
            int     rgt;
        } index_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_NUM_THREADS = 4;
        static const int MAX_NUM_THREADS = 40;

        static const char* recType;
        static const RecordObject::fieldDef_t recDef[];

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                    active;
        Thread**                indexerPid;
        Mutex                   threadMut;
        int                     threadCount;
        int                     numComplete;
        Publisher*              outQ;
        index_t                 indexRec;
        List<const char*>*      resources;
        int                     resourceEntry;
        Mutex                   resourceMut;
        Asset*                  asset;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Indexer        (lua_State* L, Asset* _asset, List<const char*>* _resources, const char* outq_name, int num_threads);
                            ~Atl03Indexer       (void);

        static void*        indexerThread       (void* parm);

        static int          luaStats            (lua_State* L);
};

#endif  /* __atl03_indexer__ */
