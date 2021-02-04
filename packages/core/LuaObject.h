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

/*
 *  The interface between Lua and C code is a virtual stack
 * 
 *      top
 *  |---------|
 *  |    a    | -1
 *  |    b    | -2
 *  |   ...   | ...
 *  |    x    |  3
 *  |    y    |  2
 *  |    z    |  1
 *  |---------|
 *    bottom
 */ 
/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"

#include <atomic>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


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
        static const int SIGNAL_COMPLETE = 0;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            LuaObject*  luaObj;
        } luaUserData_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual             ~LuaObject          (void);

        const char*         getType             (void);
        const char*         getName             (void);
        uint32_t            getTraceId          (void);

        static int          luaGetByName        (lua_State* L);
        static int          getLuaNumParms      (lua_State* L);
        static long         getLuaInteger       (lua_State* L, int parm, bool optional=false, long dfltval=0, bool* provided=NULL);
        static double       getLuaFloat         (lua_State* L, int parm, bool optional=false, double dfltval=0.0, bool* provided=NULL);
        static bool         getLuaBoolean       (lua_State* L, int parm, bool optional=false, bool dfltval=false, bool* provided=NULL);
        static const char*  getLuaString        (lua_State* L, int parm, bool optional=false, const char* dfltval=NULL, bool* provided=NULL);
        static int          returnLuaStatus     (lua_State* L, bool status, int num_obj_to_return=1);

        bool                releaseLuaObject    (void); // pairs with getLuaObject(...), returns whether object needs to be deleted

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*             ObjectType;     /* c++ class type */
        const char*             ObjectName;     /* unique identifier of object */
        const char*             LuaMetaName;    /* metatable name in lua */
        const struct luaL_Reg*  LuaMetaTable;   /* metatable in lua */  
        lua_State*              LuaState;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaObject           (lua_State* L, const char* object_type, const char* meta_name, const struct luaL_Reg meta_table[]);

        void                signalComplete      (void);
        static void         associateMetaTable  (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
        static int          createLuaObject     (lua_State* L, LuaObject* lua_obj);
        static LuaObject*   getLuaObject        (lua_State* L, int parm, const char* object_type, bool optional=false, LuaObject* dfltval=NULL);
        static LuaObject*   getLuaSelf          (lua_State* L, int parm);
        

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        uint32_t            traceId;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        /* Meta Table Functions */
        static int          luaDelete           (lua_State* L);
        static int          luaName             (lua_State* L);
        static int          luaLock             (lua_State* L);
        static int          luaWaitOn           (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Dictionary<LuaObject*>   globalObjects;
        static Mutex                    globalMut;

        std::atomic<long>               referenceCount;
        Cond                            objSignal;
        bool                            objComplete;
};

#endif  /* __lua_object__ */
