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

#ifndef __lua_engine__
#define __lua_engine__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "StringLib.h"
#include "TraceLib.h"
#include "MsgQ.h"
#include "List.h"
#include "Ordering.h"
#include "LuaObject.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * LUA ENGINE CLASS
 ******************************************************************************/

class LuaEngine
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_SELFKEY;
        static const char* LUA_ERRNO;
        static const char* LUA_TRACEID;
        static const int MAX_LUA_ARG = MAX_STR_SIZE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            PROTECTED_MODE,
            DIRECT_MODE,
            INVALID_MODE
        } mode_t;

        typedef int (*luaOpenLibFunc) (lua_State* L);

        typedef void (*luaStepHook) (lua_State *L, lua_Debug *ar);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaEngine       (const char* name, int lua_argc, char lua_argv[][MAX_LUA_ARG], uint32_t trace_id=ORIGIN, luaStepHook hook=NULL, bool paused=false); // protected mode
                            LuaEngine       (const char* name, const char* script, const char* arg, uint32_t trace_id=ORIGIN, luaStepHook hook=NULL, bool paused=false); // direct mode
                            ~LuaEngine      (void);

        static void         extend          (const char* lib_name, luaOpenLibFunc lib_func);
        static void         indicate        (const char* pkg_name, const char* pkg_version);
        static mode_t       str2mode        (const char* str);
        static const char*  mode2str        (mode_t _mode);
        static void         setErrno        (lua_State* L, int val);
        static void         setAttrBool     (lua_State* L, const char* name, bool val);
        static void         setAttrInt      (lua_State* L, const char* name, int val);
        static void         setAttrNum      (lua_State* L, const char* name, double val);
        static void         setAttrStr      (lua_State* L, const char* name, const char* val, int size=0);
        static void         setAttrFunc     (lua_State* L, const char* name, lua_CFunction val);

        const char*         getName         (void);
        bool                executeEngine   (int timeout_ms);
        bool                isActive        (void);
        void                setBoolean      (const char* name, bool val);
        void                setInteger      (const char* name, int val);
        void                setNumber       (const char* name, double val);
        void                setString       (const char* name, const char* val);
        void                setFunction     (const char* name, lua_CFunction val);
        const char*         getResult       (void);
        okey_t              lockObject      (LuaObject* lua_obj);
        void                releaseObject   (okey_t lock_key);
        bool                waitOn          (const char* signal_name, int timeout_ms);
        bool                signal          (const char* signal_name);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int ENGINE_EXIT_SIGNAL = 0;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            const char*     lib_name;
            luaOpenLibFunc  lib_func;
        } libInitEntry_t;

        typedef struct {
            const char*     pkg_name;
            const char*     pkg_version;
        } pkgInitEntry_t;

        typedef struct {
            LuaEngine*      engine;
            int             argc;
            char**          argv;
        } protectedThread_t;

        typedef struct {
            LuaEngine*      engine;
            const char*     script;
            const char*     arg;
        } directThread_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static List<libInitEntry_t> libInitTable;
        static Mutex                libInitTableMutex;

        static List<pkgInitEntry_t> pkgInitTable;
        static Mutex                pkgInitTableMutex;

        lua_State*                  L;      // lua state variable
        Mutex                       mutL;   // mutex to lua state

        const char*                 engineName;
        bool                        engineActive;
        Thread*                     engineThread;
        Cond                        engineSignal;

        Ordering<LuaObject*>        lockList;
        okey_t                      currentLockKey;

        mode_t                      mode;
        uint32_t                    traceId;
        protectedThread_t*          pInfo;
        directThread_t*             dInfo;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    protectedThread     (void* parm);
        static void*    directThread        (void* parm);
        lua_State*      createState         (luaStepHook hook);
               void     logErrorMessage     (void);

        /* Interpreter */
        static int      readlinecb          (void);
        static int      msghandler          (lua_State* L);
               int      docall              (int narg, int nres);
               char*    getprompt           (int firstline);
               int      incomplete          (int status);
               int      pushline            (int firstline);
               int      addreturn           (void);
               int      multiline           (void);
               int      loadline            (void);
               void     lprint              (void);
               void     doREPL              (void);
               int      handlescript        (const char* fname);
               int      collectargs         (char** argv, int* first);
        static int      pmain               (lua_State *L);
};

#endif  /* __lua_engine__ */
