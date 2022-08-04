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
#include "LuaEngine.h"

#include <atomic>

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
