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

#ifndef __lua_library_sys__
#define __lua_library_sys__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Ordering.h"
#include "LuaEngine.h"

/******************************************************************************
 * LUA LIBRARY SYS CLASS
 ******************************************************************************/

class LuaLibrarySys
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_SYSLIBNAME;
        static const struct luaL_Reg sysLibs [];

        static const int LUA_COMMAND_TIMEOUT = 30000; // milliseconds

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     lsys_init           (void);
        static int      luaopen_syslib      (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      lsys_version        (lua_State* L);
        static int      lsys_quit           (lua_State* L);
        static int      lsys_abort          (lua_State* L);
        static int      lsys_wait           (lua_State* L);
        static int      lsys_log            (lua_State* L);
        static int      lsys_lsmsgq         (lua_State* L);
        static int      lsys_type           (lua_State* L);
        static int      lsys_setstddepth    (lua_State* L);
        static int      lsys_setiosize      (lua_State* L);
        static int      lsys_getiosize      (lua_State* L);
        static int      lsys_seteventlvl    (lua_State* L);
        static int      lsys_geteventlvl    (lua_State* L);
        static int      lsys_lsrec          (lua_State* L);
        static int      lsys_cwd            (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int64_t launch_time;
};

#endif  /* __lua_library_sys__ */
