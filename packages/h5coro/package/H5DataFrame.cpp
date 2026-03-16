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

#include "OsApi.h"
#include "H5DataFrame.h"
#include "SystemConfig.h"
#include "GeoDataFrame.h"
#include "H5CoroLib.h"
#include "H5Object.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - H5DataFrame(<asset>, <resource>)
 *
 *  <filename> is the name of the HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int H5DataFrame::luaCreate(lua_State* L)
{
    H5Coro::Fields* _parms = NULL;
    H5Object* _h5obj = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<H5Coro::Fields*>(getLuaObject(L, 1, H5Coro::Fields::OBJECT_TYPE));
        _h5obj = dynamic_cast<H5Object*>(getLuaObject(L, 2, H5Object::OBJECT_TYPE));

        /* Create and Return Object */
        return createLuaObject(L, new H5DataFrame(L, _parms, _h5obj));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_h5obj) _h5obj->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5DataFrame::H5DataFrame (lua_State* L, H5Coro::Fields* _parms, H5Object* _h5obj, const char* group):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {}, _parms->crs.value.c_str()),
    h5obj(_h5obj),
    data(_parms->datasets, _h5obj, group, _parms->col.value, _parms->startRow.value, _parms->numRows.value)
{
    LuaEngine::setAttrFunc(L, "join", luaJoin);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DataFrame::~H5DataFrame (void)
{
    h5obj->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * luaJoin - :join(<timeout>)
 *----------------------------------------------------------------------------*/
int H5DataFrame::luaJoin (lua_State* L)
{
    bool status = true;

    try
    {
        H5DataFrame* lua_obj = dynamic_cast<H5DataFrame*>(getLuaSelf(L, 1));
        const long timeout_ms = getLuaInteger(L, 2, true, SystemConfig::settings().requestTimeoutSec.value * 1000);

        /* Join Datasets */
        lua_obj->data.joinToGDF(lua_obj, timeout_ms, true);

        /* Add Datasets */
        lua_obj->data.addToGDF(lua_obj);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to join h5 dataframe: %s", e.what());
        status = false;
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
