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

#ifndef __lua_object__
#define __lua_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Ordering.h"
#include "OsApi.h"

#include <exception>
#include <stdexcept>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * LUA EXCEPTION
 ******************************************************************************/

class LuaException : public std::runtime_error
{
   public:

        static const char* ERROR_TYPE;
        static const int ERROR_MSG_LEN = 64;

        char errmsg[ERROR_MSG_LEN];

        LuaException(const char* _errmsg, ...);
};

/******************************************************************************
 * LUA OBJECT CLASS
 ******************************************************************************/

class LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* BASE_OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            LuaObject* luaObj;
        } luaUserData_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         releaseLuaObjects   (void); // pairs with lockLuaObject(...)
        void                removeLock          (void);
        const char*         getType             (void);
        const char*         getName             (void);

        static int          getLuaNumParms      (lua_State* L);
        static long         getLuaInteger       (lua_State* L, int parm, bool optional=false, long dfltval=0, bool* provided=NULL);
        static double       getLuaFloat         (lua_State* L, int parm, bool optional=false, double dfltval=0.0, bool* provided=NULL);
        static bool         getLuaBoolean       (lua_State* L, int parm, bool optional=false, bool dfltval=false, bool* provided=NULL);
        static const char*  getLuaString        (lua_State* L, int parm, bool optional=false, const char* dfltval=NULL, bool* provided=NULL);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*         ObjectType;     /* c++ class type */
        const char*         ObjectName;     /* unique identifier of object */
        const char*         LuaMetaName;    /* metatable name in lua */
        const lua_State*    LuaState;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaObject           (lua_State* L, const char* object_type, const char* meta_name, const struct luaL_Reg meta_table[]);
        virtual             ~LuaObject          (void);

        static int          createLuaObject     (lua_State* L, LuaObject* lua_obj);
        static int          deleteLuaObject     (lua_State* L);
        static int          associateLuaName    (lua_State* L);

        static LuaObject*   lockLuaObject       (lua_State* L, int parm, const char* object_type, bool optional=false, LuaObject* dfltval=NULL);
        static int          returnLuaStatus     (lua_State* L, bool status, int num_obj_to_return=1);
        static LuaObject*   getLuaSelf          (lua_State* L, int parm);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        uint32_t                    traceId;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Ordering<LuaObject*> lockList;
        static Mutex                lockMut;
        static okey_t               currentLockKey;

        okey_t                      lockKey;
        bool                        isLocked;
};

#endif  /* __lua_object__ */
