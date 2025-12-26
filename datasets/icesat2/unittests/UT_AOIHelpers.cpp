/*
 * Copyright (c) 2025, University of Washington
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

#include "UT_AOIHelpers.h"
#include "RunTimeException.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_AOIHelpers::OBJECT_TYPE = "UT_AOIHelpers";
const char* UT_AOIHelpers::LUA_META_NAME = "UT_AOIHelpers";
const struct luaL_Reg UT_AOIHelpers::LUA_META_TABLE[] = {
    {"generate",       luaGenerateBaseline},
    {"validate",       luaValidateBaseline},
    {NULL,             NULL}
};

/******************************************************************************
 * LOCAL HELPERS
 ******************************************************************************/

static int runLuaScript(lua_State* L, bool generate)
{
    /* Inform script whether to regenerate baseline */
    lua_pushboolean(L, generate ? 1 : 0);
    lua_setglobal(L, "AOI_GENERATE_BASELINE");

    /* Execute the Lua-side test */
    const char* candidates[] = {
        "datasets/icesat2/selftests/aoi_dataframe.lua",
        "../datasets/icesat2/selftests/aoi_dataframe.lua",
        "../../datasets/icesat2/selftests/aoi_dataframe.lua"
    };

    bool ok = false;
    for(const char* path: candidates)
    {
        if(luaL_dofile(L, path) == 0)
        {
            ok = true;
            break;
        }
        else
        {
            lua_pop(L, 1); /* pop error */
        }
    }

    if(!ok)
    {
        mlog(CRITICAL, "AOI unit test failed: could not execute aoi_dataframe.lua");
        return 0;
    }

    return 1;
}

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :UT_AOIHelpers()
 *----------------------------------------------------------------------------*/
int UT_AOIHelpers::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new UT_AOIHelpers(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_AOIHelpers::UT_AOIHelpers (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_AOIHelpers::~UT_AOIHelpers(void) = default;

/*----------------------------------------------------------------------------
 * luaGenerateBaseline
 *----------------------------------------------------------------------------*/
int UT_AOIHelpers::luaGenerateBaseline (lua_State* L)
{
    (void)L;
    return returnLuaStatus(L, runLuaScript(L, true));
}

/*----------------------------------------------------------------------------
 * luaValidateBaseline
 *----------------------------------------------------------------------------*/
int UT_AOIHelpers::luaValidateBaseline (lua_State* L)
{
    (void)L;
    return returnLuaStatus(L, runLuaScript(L, false));
}
