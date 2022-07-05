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
        static const int H5_READ_TIMEOUT_MS = 30000; // 30 seconds

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
        static void         freeResources       (List<const char*>* _resources);

        static int          luaStats            (lua_State* L);
};

#endif  /* __atl03_indexer__ */
