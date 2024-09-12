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
 * INCLUDE
 ******************************************************************************/

#include "OsApi.h"
#include "GeoDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoDataFrame::OBJECT_TYPE = "GeoDataFrame";
const char* GeoDataFrame::LUA_META_NAME = "GeoDataFrame";
const struct luaL_Reg GeoDataFrame::LUA_META_TABLE[] = {
    {"export",  luaExport},
    {NULL,      NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new GeoDataFrame(L, {}));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport - export() --> lua table 
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaExport (lua_State* L)
{
    int num_rets = 1;

    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        num_rets = lua_obj->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return num_rets;
}

/*----------------------------------------------------------------------------
 * luaImport - import(<lua table>) 
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaImport (lua_State* L)
{
    bool status = true;
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        lua_obj->fromLua(L, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error importing %s: %s", OBJECT_TYPE, e.what());
        status = false;
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::GeoDataFrame(lua_State* L, const std::initializer_list<entry_t>& column_list, const std::initializer_list<entry_t>& meta_list):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary (column_list),
    metaFields (meta_list)
{
}
