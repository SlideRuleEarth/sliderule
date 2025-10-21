/*
 * Copyright (c) 2024, University of Washington
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

#include "OsApi.h"
#include "LasDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LasDataFrame::OBJECT_TYPE = "LasDataFrame";
const char* LasDataFrame::LUA_META_NAME = "LasDataFrame";
const struct luaL_Reg LasDataFrame::LUA_META_TABLE[] = {
    {"export",  luaExport},
    {NULL,      NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int LasDataFrame::luaCreate (lua_State* L)
{
    RequestFields* parms = NULL;
    GeoDataFrame* dataframe = NULL;

    try
    {
        parms = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        dataframe = dynamic_cast<GeoDataFrame*>(getLuaObject(L, 2, GeoDataFrame::OBJECT_TYPE));
        return createLuaObject(L, new LasDataFrame(L, parms, dataframe));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        if(dataframe) dataframe->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport
 *----------------------------------------------------------------------------*/
int LasDataFrame::luaExport (lua_State* L)
{
    try
    {
        LasDataFrame* lua_obj = dynamic_cast<LasDataFrame*>(getLuaSelf(L, 1));
        (void)lua_obj;
        throw RunTimeException(INFO, RTE_FAILURE, "LAS/LAZ export not implemented");
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Point cloud export stub: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/******************************************************************************
 * CONSTRUCTORS / DESTRUCTORS
 ******************************************************************************/

LasDataFrame::LasDataFrame(lua_State* L, RequestFields* _parms, GeoDataFrame* _dataframe):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    dataframe(_dataframe)
{
    assert(parms);
    assert(dataframe);
}

LasDataFrame::~LasDataFrame()
{
    parms->releaseLuaObject();
    dataframe->releaseLuaObject();
}
