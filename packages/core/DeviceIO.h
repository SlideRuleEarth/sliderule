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

#ifndef __device_io__
#define __device_io__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "DeviceObject.h"
#include "List.h"
#include "Dictionary.h"

/******************************************************************************
 * DEVICE I/O CLASS
 ******************************************************************************/

class DeviceIO: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            ioActive;
        Thread*         ioThread;
        DeviceObject*   device;
        bool            dieOnDisconnect;
        int             blockCfg;
        int             bytesProcessed;
        int             bytesDropped;
        int             packetsProcessed;
        int             packetsDropped;
        int             deviceListIndex;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    DeviceIO            (lua_State* L, DeviceObject* _device);
        virtual     ~DeviceIO           (void);

        static int  luaLogPktStats      (lua_State* L);
        static int  luaWaitOnConnect    (lua_State* L);
        static int  luaConfigBlock      (lua_State* L);
        static int  luaDieOnDisconnect  (lua_State* L);
};

#endif  /* __device_io__ */
