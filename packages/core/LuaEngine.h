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

#ifndef __lua_engine__
#define __lua_engine__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "EventLib.h"
#include "StringLib.h"
#include "List.h"

#include <atomic>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

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
        static const char* LUA_TRACEID;
        static const char* LUA_CONFDIR;
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
                            LuaEngine       (const char* script, const char* arg, uint32_t trace_id=ORIGIN, luaStepHook hook=NULL, bool paused=false); // direct mode
                            ~LuaEngine      (void);

        static void         extend          (const char* lib_name, luaOpenLibFunc lib_func);
        static void         indicate        (const char* pkg_name, const char* pkg_version);
        static const char** getPkgList      (void);
        static mode_t       str2mode        (const char* str);
        static const char*  mode2str        (mode_t _mode);
        static void         setAttrBool     (lua_State* l, const char* name, bool val);
        static void         setAttrInt      (lua_State* l, const char* name, int val);
        static void         setAttrNum      (lua_State* l, const char* name, double val);
        static void         setAttrStr      (lua_State* l, const char* name, const char* val, int size=0);
        static void         setAttrFunc     (lua_State* l, const char* name, lua_CFunction val);
        static const char*  sanitize        (const char* filename);

        uint64_t            getEngineId     (void);
        bool                executeEngine   (int timeout_ms);
        bool                isActive        (void);
        void                setBoolean      (const char* name, bool val);
        void                setInteger      (const char* name, int val);
        void                setNumber       (const char* name, double val);
        void                setString       (const char* name, const char* val);
        void                setFunction     (const char* name, lua_CFunction val);
        const char*         getResult       (void);

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

        static List<libInitEntry_t>     libInitTable;
        static Mutex                    libInitTableMutex;

        static List<pkgInitEntry_t>     pkgInitTable;
        static Mutex                    pkgInitTableMutex;

        static std::atomic<uint64_t>    engineIds;

        lua_State*                      L;      // lua state variable

        uint64_t                        engineId;
        bool                            engineActive;
        Thread*                         engineThread;
        Cond                            engineSignal;

        mode_t                          mode;
        uint32_t                        traceId;
        protectedThread_t*              pInfo;
        directThread_t*                 dInfo;

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
