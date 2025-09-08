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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <regex>

#include "LuaEngine.h"
#include "OsApi.h"
#include "EventLib.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaEngine::LUA_SELFKEY = "__this";
const char* LuaEngine::LUA_TRACEID = "__traceid";
const char* LuaEngine::LUA_CONFDIR = "__confdir";

List<LuaEngine::libInitEntry_t> LuaEngine::libInitTable;
Mutex LuaEngine::libInitTableMutex;

List<LuaEngine::pkgInitEntry_t> LuaEngine::pkgInitTable;
Mutex LuaEngine::pkgInitTableMutex;

std::atomic<uint64_t> LuaEngine::engineIds{1};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  PROTECTED_MODE
 *----------------------------------------------------------------------------*/
LuaEngine::LuaEngine(const char* name, int lua_argc, char lua_argv[][MAX_LUA_ARG], uint32_t trace_id, luaStepHook hook, bool paused)
{
    /* Initialize Parameters */
    engineId        = engineIds++;
    engineInError   = false;
    mode            = PROTECTED_MODE;
    traceId         = start_trace(CRITICAL, trace_id, "lua_engine", "{\"name\":\"%s\"}", name);
    dInfo           = NULL;
    L               = createState(hook);

    /* Create Lua Thread */
    if(lua_argc > 0) assert(lua_argv);
    pInfo = new protectedThread_t;
    pInfo->engine = this;

    /* Copy In Lua Arguments */
    pInfo->argc = MAX(lua_argc, 0) + 1; // +1 for interpreter name
    pInfo->argv = new char* [pInfo->argc + 1];
    pInfo->argv[0] = StringLib::duplicate(name);
    for(int i = 0; i < lua_argc; i++)
    {
        pInfo->argv[i + 1] = StringLib::duplicate(lua_argv[i]);
    }
    pInfo->argv[pInfo->argc] = NULL;

    /* Start Lua Thread */
    engineActive = false;
    if(!paused)
    {
        engineActive = true;
        engineThread = new Thread(protectedThread, pInfo);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  DIRECT_MODE
 *----------------------------------------------------------------------------*/
LuaEngine::LuaEngine(const char* script, const char* arg, uint32_t trace_id, luaStepHook hook, bool paused)
{
    /* Initialize Parameters */
    engineId        = engineIds++;
    engineInError   = false;
    mode            = DIRECT_MODE;
    traceId         = start_trace(CRITICAL, trace_id, "lua_engine", "{\"script\":\"%s\"}", script);
    pInfo           = NULL;
    L               = createState(hook);

    /* Create Script Thread */
    dInfo = new directThread_t;
    dInfo->engine = this;
    dInfo->script = StringLib::duplicate(script);
    dInfo->arg    = StringLib::duplicate(arg);

    /* Start Script Thread */
    engineActive = false;
    if(!paused)
    {
        engineActive = true;
        engineThread = new Thread(directThread, dInfo);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaEngine::~LuaEngine(void)
{
    engineActive = false;
    delete engineThread;

    /* Close Lua State */
    lua_close(L);

    /* Free Engine Resources */
    if(dInfo)
    {
        delete [] dInfo->script;
        delete [] dInfo->arg;
        delete dInfo;
    }

    if(pInfo)
    {
        for(int i = 0; i < pInfo->argc; i++)
        {
            delete [] pInfo->argv[i];
        }
        delete [] pInfo->argv;
        delete pInfo;
    }

    /* Stop Trace */
    stop_trace(CRITICAL, traceId);
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void LuaEngine::init(void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void LuaEngine::deinit(void)
{
    /* Free memory stored in pkgInitTable list */
    pkgInitTableMutex.lock();
    {
        const int num_pkgs = pkgInitTable.length();
        for(int i = 0; i < num_pkgs; i++)
        {
            delete [] pkgInitTable[i].pkg_name;
            delete [] pkgInitTable[i].pkg_version;
        }
    }
    pkgInitTableMutex.unlock();

    /* Free memory stored libInitTable list */
    libInitTableMutex.lock();
    {
        const int num_libs = libInitTable.length();
        for(int i = 0; i < num_libs ; i++)
        {
            delete [] libInitTable[i].lib_name;
        }
    }
    libInitTableMutex.unlock();
}

/*----------------------------------------------------------------------------
 * extend
 *----------------------------------------------------------------------------*/
void LuaEngine::extend(const char* lib_name, luaOpenLibFunc lib_func)
{
    libInitTableMutex.lock();
    {
        libInitEntry_t entry;
        entry.lib_name = StringLib::duplicate(lib_name);
        entry.lib_func = lib_func;
        libInitTable.add(entry);
    }
    libInitTableMutex.unlock();
}

/*----------------------------------------------------------------------------
 * extend
 *----------------------------------------------------------------------------*/
void LuaEngine::indicate(const char* pkg_name, const char* pkg_version)
{
    pkgInitTableMutex.lock();
    {
        pkgInitEntry_t entry;
        entry.pkg_name = StringLib::duplicate(pkg_name);
        entry.pkg_version = StringLib::duplicate(pkg_version);
        pkgInitTable.add(entry);
    }
    pkgInitTableMutex.unlock();
}

/*----------------------------------------------------------------------------
 * getPkgList
 *----------------------------------------------------------------------------*/
const char** LuaEngine::getPkgList(void)
{
    const char** pkg_list = NULL;

    pkgInitTableMutex.lock();
    {
        const int num_pkgs = pkgInitTable.length();
        if(num_pkgs > 0)
        {
            pkg_list = new const char* [num_pkgs + 1];
            for(int i = 0; i < num_pkgs; i++)
            {
                pkg_list[i] = StringLib::duplicate(pkgInitTable[i].pkg_name);
            }
            pkg_list[num_pkgs] = NULL; // null terminate list
        }
    }
    pkgInitTableMutex.unlock();

    return pkg_list;
}

/*----------------------------------------------------------------------------
 * str2mode
 *----------------------------------------------------------------------------*/
LuaEngine::mode_t LuaEngine::str2mode(const char* str)
{
    if(StringLib::match(str, "PROTECTED"))      return PROTECTED_MODE;
    if(StringLib::match(str, "DIRECT"))         return DIRECT_MODE;

    return INVALID_MODE;
}

/*----------------------------------------------------------------------------
 * mode2str
 *----------------------------------------------------------------------------*/
const char* LuaEngine::mode2str(mode_t _mode)
{
    switch(_mode)
    {
        case PROTECTED_MODE:    return "PROTECTED";
        case DIRECT_MODE:       return "DIRECT";
        default:                return "INVALID";
    }
}

/*----------------------------------------------------------------------------
 * setAttrBool
 *----------------------------------------------------------------------------*/
void LuaEngine::setAttrBool (lua_State* l, const char* name, bool val)
{
    if(l == NULL) return;
    lua_pushstring(l, name);
    lua_pushboolean(l, val);
    lua_settable(l, -3);
}

/*----------------------------------------------------------------------------
 * setAttrInt
 *----------------------------------------------------------------------------*/
void LuaEngine::setAttrInt (lua_State* l, const char* name, long long val)
{
    if(l == NULL) return;
    lua_pushstring(l, name);
    lua_pushinteger(l, val);
    lua_settable(l, -3);
}

/*----------------------------------------------------------------------------
 * setAttrNum
 *----------------------------------------------------------------------------*/
void LuaEngine::setAttrNum (lua_State* l, const char* name, double val)
{
    if(l == NULL) return;
    lua_pushstring(l, name);
    lua_pushnumber(l, val);
    lua_settable(l, -3);
}

/*----------------------------------------------------------------------------
 * setAttrStr
 *----------------------------------------------------------------------------*/
void LuaEngine::setAttrStr (lua_State* l, const char* name, const char* val, int size)
{
    if(l == NULL) return;
    lua_pushstring(l, name);
    if(size > 0)    lua_pushlstring(l, val, size);
    else            lua_pushstring(l, val);
    lua_settable(l, -3);
}

/*----------------------------------------------------------------------------
 * setAttrFunc
 *----------------------------------------------------------------------------*/
void LuaEngine::setAttrFunc (lua_State* l, const char* name, lua_CFunction val)
{
    if(l == NULL) return;
    lua_pushstring(l, name);
    lua_pushcfunction(l, val);
    lua_settable(l, -3);
}

/*----------------------------------------------------------------------------
 * showStack
 *
 * Note: does not change the stack
 *----------------------------------------------------------------------------*/
void LuaEngine::showStack (lua_State* l, const char* prefix)
{
    if(l == NULL) return;
    const int top = lua_gettop(l);

    if( prefix ) print2term("%s, stack depth is: %d\n", prefix, top );
    else         print2term("stack depth is: %d\n", top );

    for (int i = top; i >= 1; i--)
    {
        const int t = lua_type(l, i);

        switch (t)
        {
            case LUA_TSTRING:
                print2term("--%02d-- string: \'%s\'\n", i, lua_tostring(l, i));
                break;
            case LUA_TBOOLEAN:
                print2term("--%02d-- boolean: %s\n", i, lua_toboolean(l, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                print2term("--%02d-- number: %g\n", i, lua_tonumber(l, i));
                break;
            default:
                print2term("--%02d-- %s\n", i, lua_typename(l, t));
                break;
        }
    }
    print2term("\n");
}

/*----------------------------------------------------------------------------
 * sanitize
 *
 *  Note: must delete returned string
 *----------------------------------------------------------------------------*/
const char* LuaEngine::sanitize (const char* filename)
{
    char* safe_filename = StringLib::duplicate(filename);
    StringLib::replace(safe_filename, PATH_DELIMETER, '_');
    const FString safe_pathname("%s%c%s%c%s.lua", CONFDIR, PATH_DELIMETER, "api", PATH_DELIMETER, safe_filename);
    delete [] safe_filename;
    return safe_pathname.c_str(true);
}

/*----------------------------------------------------------------------------
 * luaAbortHook
 *----------------------------------------------------------------------------*/
void LuaEngine::abortHook (lua_State* l, lua_Debug *ar)
{
    if(l == NULL) return;
    (void)ar;
    lua_pushstring(l, LUA_SELFKEY);
    lua_gettable(l, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = static_cast<LuaEngine*>(lua_touserdata(l, -1));
    if(!li)
    {
        luaL_error(l, "Unable to access Lua engine - aborting!");
    }
    else if(!li->engineActive)
    {
        char error_msg[512];
        StringLib::format(error_msg, 256, "Lua engine no longer active - exiting script <%s>", li->dInfo != NULL ? li->dInfo->script : li->pInfo->argv[0]);
        luaL_error(l, error_msg);
    }
}

/*----------------------------------------------------------------------------
 * getLuaState
 *----------------------------------------------------------------------------*/
lua_State* LuaEngine::getLuaState (void)
{
    return L;
}

 /*----------------------------------------------------------------------------
 * getEngineId
 *----------------------------------------------------------------------------*/
uint64_t LuaEngine::getEngineId(void) const
{
    return engineId;
}

/*----------------------------------------------------------------------------
 * executeEngine
 *----------------------------------------------------------------------------*/
bool LuaEngine::executeEngine(int timeout_ms)
{
    bool status = false;

    engineSignal.lock();
    {
        if(!engineActive)
        {
            /* Execute Engine */
            if(mode == PROTECTED_MODE)
            {
                engineActive = true;
                engineThread = new Thread(protectedThread, pInfo);
            }
            else if(mode == DIRECT_MODE)
            {
                engineActive = true;
                engineThread = new Thread(directThread, dInfo);
                if(timeout_ms != IO_CHECK)
                {
                    engineSignal.wait(ENGINE_EXIT_SIGNAL, timeout_ms);
                }
            }

            /*
             * completion is true if engine no longer active
             * and if there are no errors
             */
            status = (!engineActive) && (!engineInError);

            /* Reset Error State */
            engineInError = false;
        }
    }
    engineSignal.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * isActive
 *----------------------------------------------------------------------------*/
bool LuaEngine::isActive(void) const
{
    return engineActive;
}

/*----------------------------------------------------------------------------
 * setBoolean
 *----------------------------------------------------------------------------*/
void LuaEngine::setBoolean (const char* name, bool val)
{
    engineSignal.lock();
    {
        lua_pushboolean(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * setInteger
 *----------------------------------------------------------------------------*/
void LuaEngine::setInteger (const char* name, long val)
{
    engineSignal.lock();
    {
        lua_pushinteger(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * setNumber
 *----------------------------------------------------------------------------*/
void LuaEngine::setNumber (const char* name, double val)
{
    engineSignal.lock();
    {
        lua_pushnumber(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * setString
 *----------------------------------------------------------------------------*/
void LuaEngine::setString (const char* name, const char* val)
{
    engineSignal.lock();
    {
        lua_pushstring(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * setFunction
 *----------------------------------------------------------------------------*/
void LuaEngine::setFunction (const char* name, lua_CFunction val)
{
    engineSignal.lock();
    {
        lua_pushcfunction(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * setObject
 *----------------------------------------------------------------------------*/
void LuaEngine::setObject (const char* name, void* val)
{
    engineSignal.lock();
    {
        lua_pushlightuserdata(L, val);
        lua_setglobal(L, name);
    }
    engineSignal.unlock();
}

/*----------------------------------------------------------------------------
 * getResult
 *----------------------------------------------------------------------------*/
const char* LuaEngine::getResult (bool* in_error)
{
    if(in_error != NULL)
    {
        if( (lua_gettop(L) >= 2) && // the lua context has a variable for the error status
            (lua_isboolean(L, 2)) ) // the variable is of the boolean type
        {
            *in_error = !lua_toboolean(L, 2); // set the error status
        }
        else
        {
            *in_error = false; // default to no error if not provided
        }
    }

    if( (lua_gettop(L) >= 1) && // the lua context has a variable for the results
        lua_isstring(L, 1) )    // the result variable is a string
    {
        return lua_tostring(L, 1); // return results
    }

    return NULL; // return null to indicate results were no obtainable
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * protectedThread
 *----------------------------------------------------------------------------*/
void* LuaEngine::protectedThread (void* parm)
{
    protectedThread_t* p = static_cast<protectedThread_t*>(parm);

    p->engine->engineSignal.lock();
    {
        /* Run Script */
        lua_pushcfunction(p->engine->L, &pmain);            /* to call 'pmain' in protected mode */
        lua_pushinteger(p->engine->L, p->argc);             /* 1st argument */
        lua_pushlightuserdata(p->engine->L, p->argv);       /* 2nd argument */
        const int status = lua_pcall(p->engine->L, 2, 1, 0);/* do the call */
        const int result = lua_toboolean(p->engine->L, -1); /* get result */
        if (status != LUA_OK || result == 0)
        {
            mlog(CRITICAL, "%s exited with error", p->argc > 0 ? p->argv[0] : "lua script");
            p->engine->engineInError = true;
        }
        else
        {
            mlog(DEBUG, "%s executed", p->argc > 0 ? p->argv[0] : "lua script");
        }

        /* Set Inactive */
        p->engine->engineActive = false;
        p->engine->engineSignal.signal(ENGINE_EXIT_SIGNAL);
    }
    p->engine->engineSignal.unlock();

    return NULL;
}

/*----------------------------------------------------------------------------
 * directThread
 *----------------------------------------------------------------------------*/
void* LuaEngine::directThread (void* parm)
{
    directThread_t* d = static_cast<directThread_t*>(parm);

    d->engine->engineSignal.lock();
    {
        lua_State* L = d->engine->L;

        /* Create Arg Table */
        lua_createtable(L, 1, 0);
        lua_pushstring(L, d->arg);
        lua_rawseti(L, -2, 1);
        lua_setglobal(L, "arg");

        /* Execute Script */
        int status = luaL_loadfile(L, d->script);
        if(status == LUA_OK)
        {
            status = lua_pcall(L, 0, LUA_MULTRET, 0);
        }

        /* Log Error */
        if (status != LUA_OK)
        {
            d->engine->engineInError = true;
            d->engine->logErrorMessage();
        }

        /* Set Inactive */
        d->engine->engineActive = false;
        d->engine->engineSignal.signal(ENGINE_EXIT_SIGNAL);
    }
    d->engine->engineSignal.unlock();

    return NULL;
}

/*----------------------------------------------------------------------------
 * createState
 *----------------------------------------------------------------------------*/
lua_State* LuaEngine::createState(luaStepHook hook)
{
    /* Initialize Lua */
    lua_State* l = luaL_newstate();     /* opens Lua */
    assert(l);                          /* not enough memory to create lua state */
    if(hook) lua_sethook(l, hook, LUA_MASKLINE, 0);

    /* Register Interpreter Object */
    lua_pushstring(l, LUA_SELFKEY);
    lua_pushlightuserdata(l, static_cast<void*>(this));
    lua_settable(l, LUA_REGISTRYINDEX); /* registry[LUA_SELFKEY] = this */

    /* Register Application Libraries */
    libInitTableMutex.lock();
    {
        for(int i = 0; i < libInitTable.length(); i++)
        {
            luaL_requiref(l, libInitTable[i].lib_name, libInitTable[i].lib_func, 1);
            lua_pop(l, 1);
        }
    }
    libInitTableMutex.unlock();

    /* Register Package Versions */
    pkgInitTableMutex.lock();
    {
        for(int i = 0; i < pkgInitTable.length(); i++)
        {
            char pkg_name[MAX_STR_SIZE];
            StringLib::format(pkg_name, MAX_STR_SIZE, "__%s__", pkgInitTable[i].pkg_name);
            lua_pushstring(l, pkgInitTable[i].pkg_version);
            lua_setglobal(l, pkg_name);
        }
    }
    pkgInitTableMutex.unlock();

    /* Open Libraries */
    lua_pushboolean(l, 1);  /* signal for libraries to ignore env. vars. */
    lua_setfield(l, LUA_REGISTRYINDEX, "LUA_NOENV");
    luaL_openlibs(l);  /* open standard libraries */

    /* Set Trace ID */
    lua_pushnumber(l, traceId);
    lua_setglobal(l, LUA_TRACEID);

    /* Set Configuration Directory */
    lua_pushstring(l, CONFDIR);
    lua_setglobal(l, LUA_CONFDIR);

    /* Set Starting Lua Path */
    const FString lpath("%s/ext/?.lua;%s/api/?.lua", CONFDIR, CONFDIR);
    lua_getglobal(l, "package" );
    lua_getfield(l, -1, "path" ); // get field "path" from table at top of stack (-1)
    lua_pop(l, 1 ); // get rid of the string on the stack we just pushed on line 5
    lua_pushstring(l, lpath.c_str()); // push the new one
    lua_setfield(l, -2, "path" ); // set the field "path" in table at -2 with value at top of stack
    lua_pop(l, 1 ); // get rid of package table from top of stack

    /* Return State */
    return l;
}

/*----------------------------------------------------------------------------
 * logErrorMessage
 *----------------------------------------------------------------------------*/
void LuaEngine::logErrorMessage (void)
{
    /*
    ** Check whether 'status' is not OK and, if so, logs the error
    ** message on the top of the stack. It assumes that the error object
    ** is a string, as it was either generated by Lua or by 'msghandler'.
    */
    char msg[MAX_STR_SIZE];
    StringLib::format(msg, MAX_STR_SIZE, "%s", lua_tostring(L, -1));

    /* Cannot log here because console monitor may be garbage collected
     * as a result of the script error; so instead write to stderr. */
    // mlog(CRITICAL, "%s", msg);
    fprintf(stderr, "%s\n", msg);

    /* remove message */
    lua_pop(L, 1);
}

/******************************************************************************
 * LUA COMMAND LINE INTERPRETER
 ******************************************************************************
 * The command line code below is taken and modified from the Lua 5.3 release
 * in the following ways:
 *  1. Modified pmain for safe execution
 *      - does not execute LUA_INIT
 *      - libraries are opened prior to call and are not opened here
 *      - does not allow -e or -l options to execute command line code
 *  2. Changed most functions to non-static class methods so that multiple
 *      interpreters could run independently (the original code used one or
 *      two global variables that made it not thread safe).
 *  3. Readline is required and the code that was used when readline is not
 *      present is removed.
 *  4. Minor reformatting to make it consistent with the style used in the
 *      rest of this module.
 *
 * The original source code is released under the terms of the MIT license
 * reproduced here:
 *
 * Copyright (c) 1994–2019 Lua.org, PUC-Rio.
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#include <readline/readline.h>
#include <readline/history.h>

/* used in interactive mode only - see note in pmain */
LuaEngine* lua_readline_interpreter = NULL;

#define LUA_PROMPT		  "> "
#define LUA_PROMPT2		  ">> "
#define LUA_MAXINPUT		512

/* bits of various argument indicators in 'args' */
#define has_error	      1	/* bad option */
#define has_i		        2	/* -i */
#define has_v		        4	/* -v */
#define has_e		        8	/* -e */
#define has_E		        16	/* -E */

/* mark in error messages for incomplete statements */
#define EOFMARK		      "<eof>"
#define marklen		      (sizeof(EOFMARK)/sizeof(char) - 1)

/*----------------------------------------------------------------------------
 * readlinecb
 *----------------------------------------------------------------------------*/
int LuaEngine::readlinecb(void)
{
    if(lua_readline_interpreter)
    {
        if(!lua_readline_interpreter->engineActive)
        {
            /* Push control-d onto input buffer
             * ... this is used for interactive mode to terminate
             * the readline input loop
             */
            rl_stuff_char(4);
        }
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * msghandler
 *
 *  Message handler used to run all chunks
 *----------------------------------------------------------------------------*/
int LuaEngine::msghandler (lua_State* l)
{
    const char *msg = lua_tostring(l, 1);
    if (msg == NULL)
    {
        /* does it have a metamethod that produces a string? */
        if (luaL_callmeta(l, 1, "__tostring") && lua_type(l, -1) == LUA_TSTRING)
        {
            return 1;  /* that is the message */
        }
        msg = lua_pushfstring(l, "(error object is a %s value)", luaL_typename(l, 1));
    }
    luaL_traceback(l, l, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

/*----------------------------------------------------------------------------
 * docall
 *
 *  Interface to 'lua_pcall', which sets appropriate message function
 *  and C-signal handler. Used to run all chunks.
 *----------------------------------------------------------------------------*/
int LuaEngine::docall (int narg, int nres)
{
    int status;
    const int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

/*----------------------------------------------------------------------------
 * getprompt
 *
 *  Returns the string to be used as a prompt by the interpreter.
 *----------------------------------------------------------------------------*/
char* LuaEngine::getprompt (int firstline)
{
    const char *p;
    lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    return const_cast<char*>(p);
}

/*----------------------------------------------------------------------------
 * incomplete
 *
 *  Check whether 'status' signals a syntax error and the error
 *  message at the top of the stack ends with the above mark for
 *  incomplete statements.
 *----------------------------------------------------------------------------*/
int LuaEngine::incomplete (int status)
{
    if (status == LUA_ERRSYNTAX)
    {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && StringLib::match(msg + lmsg - marklen, EOFMARK))
        {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------
 * pushline
 *
 *  Prompt the user, read a line, and push it into the Lua stack.
 *----------------------------------------------------------------------------*/
int LuaEngine::pushline (int firstline)
{
    size_t l;
    const char* prmt = getprompt(firstline);

    if(!engineActive) return 0; // exit REPL
    char* b = readline(prmt);

    if (b == NULL) return 0;  /* no input (prompt will be popped by caller) */
    lua_pop(L, 1);  /* remove prompt */
    l = StringLib::size(b);
    if (l > 0 && b[l-1] == '\n')    b[--l] = '\0'; /* line ends with newline? remove it */
    if (firstline && b[0] == '=')   lua_pushfstring(L, "return %s", b + 1); /* for compatibility with 5.2, change '=' to 'return' */
    else                            lua_pushlstring(L, b, l);
    free(b); // NOLINT
    return 1;
}

/*----------------------------------------------------------------------------
 * pushline
 *
 *  Try to compile line on the stack as 'return <line>;'; on return, stack
 *  has either compiled chunk or original line (if compilation failed).
 *----------------------------------------------------------------------------*/
int LuaEngine::addreturn (void)
{
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    const int status = luaL_loadbuffer(L, retline, StringLib::size(retline), "=stdin");
    if (status == LUA_OK)
    {
        lua_remove(L, -2);  /* remove modified line */
        if (line[0] != '\0') add_history(line); /* non empty? keep history */
    }
    else
    {
        lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
    }
    return status;
}

/*----------------------------------------------------------------------------
 * multiline
 *
 *  Read multiple lines until a complete Lua statement
 *----------------------------------------------------------------------------*/
int LuaEngine::multiline (void)
{
    /* repeat until gets a complete statement */
    for (;;)
    {
        size_t len;
        const char *line = lua_tolstring(L, 1, &len);  /* get what it has */
        const int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
        if (!incomplete(status) || !pushline(0))
        {
            add_history(line);  /* keep history */
            return status;  /* cannot or should not try to add continuation line */
        }
        lua_pushliteral(L, "\n");  /* add newline... */
        lua_insert(L, -2);  /* ...between the two lines */
        lua_concat(L, 3);  /* join them */
    }
}

/*----------------------------------------------------------------------------
 * multiline
 *
 *  Read a line and try to load (compile) it first as an expression (by
 *  adding "return " in front of it) and second as a statement. Return
 *  the final status of load/call with the resulting function (if any)
 *  in the top of the stack.
 *----------------------------------------------------------------------------*/
int LuaEngine::loadline (void)
{
    int status;
    lua_settop(L, 0);

    /* no input */
    if (!pushline(1)) return -1;

    /* 'return ...' did not work? try as command, maybe with continuation lines */
    if ((status = addreturn()) != LUA_OK) status = multiline();

    /* remove line from the stack */
    lua_remove(L, 1);
    lua_assert(lua_gettop(L) == 1);
    return status;
}

/*----------------------------------------------------------------------------
 * lprint
 *
 *  Prints (calling the Lua 'print' function) any values on the stack
 *----------------------------------------------------------------------------*/
void LuaEngine::lprint (void)
{
    const int n = lua_gettop(L);

    /* any result to be printed? */
    if (n > 0)
    {
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
        {
            lua_writestringerror("%s\n", lua_pushfstring(L, "error calling 'print' (%s)", lua_tostring(L, -1)));
        }
    }
}

/*----------------------------------------------------------------------------
 * doREPL
 *
 *  Do the REPL: repeatedly read (load) a line, evaluate (call) it, and print any results.
 *----------------------------------------------------------------------------*/
void LuaEngine::doREPL (void)
{
    int status;
    while ((status = loadline()) != -1)
    {
        if (status == LUA_OK)   status = docall(0, LUA_MULTRET);
        if (status == LUA_OK)   lprint();
        else                    logErrorMessage();
    }
    lua_settop(L, 0);  /* clear stack */
    lua_writeline();
}

/*----------------------------------------------------------------------------
 * handlescript
 *----------------------------------------------------------------------------*/
int LuaEngine::handlescript (const char* fname)
{
    int status = luaL_loadfile(L, fname);
    if (status == LUA_OK)
    {
        int i;
        int n;
        if (lua_getglobal(L, "arg") != LUA_TTABLE)
        {
            luaL_error(L, "'arg' is not a table");
        }
        n = (int)luaL_len(L, -1);
        luaL_checkstack(L, n + 3, "too many arguments to script");
        for (i = 1; i <= n; i++)
        {
            lua_rawgeti(L, -i, i);
        }
        lua_remove(L, -i);  /* remove table from the stack */
        status = docall(n, LUA_MULTRET);
    }

    if (status != LUA_OK) logErrorMessage();
    return status;
}

/*----------------------------------------------------------------------------
 * collectargs
 *
 *  Traverses all arguments from 'argv', returning a mask with those
 *  needed before running any Lua code (or an error code if it finds
 *  any invalid argument). 'first' returns the first not-handled argument
 *  (either the script name or a bad argument in case of error).
 *----------------------------------------------------------------------------*/
int LuaEngine::collectargs (char **argv, int *first)
{
    int args = 0;
    int i;
    for (i = 1; argv[i] != NULL; i++)
    {
      *first = i;
      if (argv[i][0] != '-')  /* not an option? */
          return args;  /* stop handling options */
      switch (argv[i][1]) {  /* else check option */
        case '-':  /* '--' */
          if (argv[i][2] != '\0')  /* extra characters after '--'? */
            return has_error;  /* invalid option */
          *first = i + 1;
          return args;
        case '\0':  /* '-' */
          return args;  /* script "name" is '-' */
        case 'E':
          if (argv[i][2] != '\0')  /* extra characters after 1st? */
            return has_error;  /* invalid option */
          args |= has_E;
          break;
        case 'i':
          args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
        case 'v':
          if (argv[i][2] != '\0')  /* extra characters after 1st? */
            return has_error;  /* invalid option */
          args |= has_v;
          break;
        case 'e':
          args |= has_e;  /* FALLTHROUGH */
        case 'l':  /* both options need an argument */
          if (argv[i][2] == '\0') {  /* no concatenated argument? */
            i++;  /* try next 'argv' */
            if (argv[i] == NULL || argv[i][0] == '-')
              return has_error;  /* no next argument or it is another option */
          }
          break;
        default:  /* invalid option */
          return has_error;
      }
    }
    *first = i;  /* no script name */
    return args;
}

/*----------------------------------------------------------------------------
 * pmain - protected mode
 *----------------------------------------------------------------------------*/
int LuaEngine::pmain (lua_State *L)
{
    /* retrieve LuaEngine object from registry */
    lua_pushstring(L, LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = static_cast<LuaEngine*>(lua_touserdata(L, -1));
    if(!li)
    {
        mlog(CRITICAL, "Unable to access lua interpreter");
        lua_pushboolean(L, 0);  /* error */
        return 0;
    }

    /* parse arguments */
    const int argc    = static_cast<int>(lua_tointeger(L, 1));
    char**    argv    = static_cast<char**>(lua_touserdata(L, 2));
    int       script  = 0;
    const int args    = collectargs(argv, &script);

    /* check that interpreter has correct version */
    luaL_checkversion(L);

    /* check if error in processing arguments */
    if (args == has_error)
    {
        mlog(CRITICAL, "Invalid parameters passed to lua srcipt!");
        lua_pushboolean(L, 0);  /* error */
        return 0;
    }

    /*
    ** Create the 'arg' table, which stores all arguments from the
    ** command line ('argv'). It should be aligned so that, at index 0,
    ** it has 'argv[script]', which is the script name. The arguments
    ** to the script (everything after 'script') go to positive indices;
    ** other arguments (before the script name) go to negative indices.
    ** If there is no script name, assume interpreter's name as base.
    */
    int nscript = script;
    if (script == argc) nscript = 0;  /* no script name? */
    const int narg = argc - (nscript + 1);  /* number of positive indices */
    lua_createtable(L, narg, nscript + 1);
    for (int i = 0; i < argc; i++)
    {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - nscript);
    }
    lua_setglobal(L, "arg");

    /* execute main script (if there is one) */
    if (script < argc)
    {
        const int script_status = li->handlescript(argv[script]);
        if( script_status != LUA_OK)
        {
            lua_pushboolean(L, 0);  /* error */
            return 0;
        }
    }

    /* start interactive interpreter */
    if (args & has_i)
    {
        /* Setup Interactive Mode Variables
         *   ... this uses global variables that associate a specific interpreter
         *   with the readline library.  There can only be one interactive
         *   readline interface and the application developer needs to be
         *   aware of this limitation.  Opening up multiple LuaLibrarys
         *   in interactive mode will produce undefined behavior.  A future
         *   improvement will be to enforce this limitation.
         */
        lua_readline_interpreter = li; /* install current engine for readline support */
        rl_event_hook = readlinecb; /* install call-back for readline */

        /* Start Interactive Mode
         * lua_writestring(LUA_COPYRIGHT, StringLib::size(LUA_COPYRIGHT));
         * lua_writeline();
         */
        OsApi::sleep(1);
        li->doREPL();  /* do read-eval-print loop */
    }

    /* return success */
    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}
