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

#include "LuaEngine.h"
#include "Atl03Granule.h"

#include "Atl24Runner.h"
#include "Atl24Uncertainty.h"
#include "Atl24Writer.h"
#include "BlunderRunner.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_ATL24_LIBNAME    "atl24"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * atl24_version
 *----------------------------------------------------------------------------*/
int atl24_version (lua_State* L)
{
    /* Check for Verbose */
    bool verbose = false;
    if(lua_isboolean(L, 1))
    {
        verbose = lua_toboolean(L, 1);
    }

    /* Display Information on Terminal */
    if(verbose)
    {
        print2term("Package:    %s\n", LUA_ATL24_LIBNAME);
        print2term("Version:    %s\n", BINID);
        print2term("Build:      %s\n", BUILDINFO);
        print2term("Algorithm:  %s\n", ALGOINFO);
    }

    /* Return Information to Lua */
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    lua_pushstring(L, ALGOINFO);
    return 3;
}

/*----------------------------------------------------------------------------
 * atl24_open
 *----------------------------------------------------------------------------*/
int atl24_open (lua_State *L)
{
    static const struct luaL_Reg atl24_functions[] = {
        {"version",         atl24_version},
        {"blunder",         BlunderRunner::luaCreate},
        {"classifier",      Atl24Runner::luaCreate},
        {"writer",          Atl24Writer::luaCreate},
        {"uncertainty",     Atl24Uncertainty::luaCreate},
        {"atl03granule",    Atl03Granule::luaCreate},
        {NULL,              NULL}
    };

    luaL_newlib(L, atl24_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initatl24 (void)
{
    /* Initialize Modules */
    Atl24Uncertainty::init();
    Atl24Writer::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_ATL24_LIBNAME, atl24_open, LIBID);

    /* Display Status */
    print2term("%s package initialized (library=%s, plugin=%s)\n", LUA_ATL24_LIBNAME, LIBID, BINID);
}

void deinitatl24 (void)
{
}
}
