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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "gedi.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEDI_LIBNAME    "gedi"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * gedi_version
 *----------------------------------------------------------------------------*/
int gedi_version (lua_State* L)
{
    /* Display Version Information on Terminal */
    print2term("GEDI Plugin Version: %s\n", BINID);
    print2term("Build Information: %s\n", BUILDINFO);

    /* Return Version Information to Lua */
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * gedi_open
 *----------------------------------------------------------------------------*/
int gedi_open (lua_State *L)
{
    static const struct luaL_Reg gedi_functions[] = {
        {"parms",               GediParms::luaCreate},
        {"gedi01b",             Gedi01bReader::luaCreate},
        {"gedi02a",             Gedi02aReader::luaCreate},
        {"gedi04a",             Gedi04aReader::luaCreate},
        {"version",             gedi_version},
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, gedi_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "NUM_BEAMS",       GediParms::NUM_BEAMS);
    LuaEngine::setAttrInt(L, "ALL_BEAMS",       GediParms::ALL_BEAMS);
    LuaEngine::setAttrInt(L, "RQST_TIMEOUT",    GediParms::DEFAULT_RQST_TIMEOUT);
    LuaEngine::setAttrInt(L, "NODE_TIMEOUT",    GediParms::DEFAULT_NODE_TIMEOUT);
    LuaEngine::setAttrInt(L, "READ_TIMEOUT",    GediParms::DEFAULT_READ_TIMEOUT);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgedi (void)
{
    /* Initialize Modules */
    Gedi01bReader::init();
    Gedi02aReader::init();
    Gedi04aReader::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_GEDI_LIBNAME, gedi_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEDI_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_GEDI_LIBNAME, BINID);
}

void deinitgedi (void)
{
}
}
