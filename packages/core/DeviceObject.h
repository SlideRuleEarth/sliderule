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

#ifndef __device_object__
#define __device_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Ordering.h"
#include "OsApi.h"
#include "LuaObject.h"

/******************************************************************************
 * DEVICE OBJECT CLASS
 ******************************************************************************/

class DeviceObject: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            READER,
            WRITER,
            DUPLEX
        } role_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const role_t role;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            DeviceObject        (lua_State* L, role_t _role);
        virtual             ~DeviceObject       (void);
        static char*        getDeviceList       (void);
        static int          luaList             (lua_State* L);

        virtual bool        isConnected         (int num_connections) = 0;
        virtual void        closeConnection     (void) = 0;
        virtual int         writeBuffer         (const void* buf, int len) = 0;
        virtual int         readBuffer          (void* buf, int len) = 0;
        virtual int         getUniqueId         (void) = 0;
        virtual const char* getConfig           (void) = 0;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Ordering<DeviceObject*>  deviceList;
        static Mutex                    deviceListMut;
        static okey_t                   currentListKey;

        okey_t                          deviceListKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaSend             (lua_State* L);
        static int          luaReceive          (lua_State* L);
        static int          luaConfig           (lua_State* L);
        static int          luaIsConnected      (lua_State* L);
        static int          luaClose            (lua_State* L);
};

#endif  /* __device_object__ */
