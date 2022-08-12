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
        virtual int         writeBuffer         (const void* buf, int len, int timeout=SYS_TIMEOUT) = 0;
        virtual int         readBuffer          (void* buf, int len, int timeout=SYS_TIMEOUT) = 0;
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
