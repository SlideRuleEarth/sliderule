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

#ifndef __h5_proxy__
#define __h5_proxy__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "RecordObject.h"

/******************************************************************************
 * H5 PROXY CLASS
 ******************************************************************************/

class H5Proxy: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const int MAX_RQST_STR_SIZE = 128;

        static const char* recType;
        static const RecordObject::fieldDef_t recDef[];

        static const char* REQUEST_QUEUE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            READ = 0
        } operation_t;

        typedef struct {
            int port;
        } client_info_t;

        typedef struct {
            uint32_t    id;
            uint32_t    operation;
            char        url[MAX_RQST_STR_SIZE];
            char        datasetname[MAX_RQST_STR_SIZE];
            uint32_t    valtype;
            int64_t     col;
            int64_t     startrow;
            int64_t     numrows;
        } request_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaCreate       (lua_State* L);
        static int              luaConnect      (lua_State* L);
        static int              luaDisconnect   (lua_State* L);
        static void             init            (void);
        static H5Lib::info_t    read            (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows);

                                H5Proxy         (lua_State* L, const char* _ip_addr, int _port);
                                ~H5Proxy        (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* Client Data */
        static Mutex                clientMut;
        static bool                 clientsActive;
        static List<Thread*>        clientThreadPool;
        static Publisher*           clientRequestQ;

        /* Server Data */
        bool                        active;
        Thread*                     pid;
        char*                       ipAddr;
        int                         port;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    clientThread    (void* parm);
        static void*    requestThread   (void* parm);
};

#endif  /* __h5_proxy__ */
