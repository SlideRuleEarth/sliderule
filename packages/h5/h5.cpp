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

#include "H5Coro.h"
#include "H5Array.h"
#include "H5DArray.h"
#include "H5Element.h"
#include "H5File.h"
#include "H5DatasetDevice.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_H5_LIBNAME  "h5"

#ifndef H5CORO_THREAD_POOL_SIZE
#define H5CORO_THREAD_POOL_SIZE 128
#endif

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * h5_read(asset, resource, variable, dtype)
 *----------------------------------------------------------------------------*/
int h5_read (lua_State* L)
{
    Asset* asset = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(LuaObject::getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = LuaObject::getLuaString(L, 2);
        const char* variable = LuaObject::getLuaString(L, 3);
        const RecordObject::fieldType_t dtype = static_cast<RecordObject::fieldType_t>(LuaObject::getLuaInteger(L, 4));
        const int timeout = LuaObject::getLuaInteger(L, 5, true, 600 * 1000); // milliseconds

        /* Create H5 Context */
        H5Coro::Context context(asset, resource);
        /* Perform Read */
        switch(dtype)
        {
            case RecordObject::INT8:    
            {
                H5Element<int8_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::INT16: 
            {
                H5Element<int16_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }            
            case RecordObject::INT32: 
            {
                H5Element<int32_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::INT64: 
            {
                H5Element<int64_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::UINT8: 
            {
                H5Element<uint8_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::UINT16:
            {
                H5Element<uint16_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::UINT32:
            {
                H5Element<uint32_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::UINT64:
            {
                H5Element<uint64_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::FLOAT: 
            {
                H5Element<float> element(&context, variable);
                element.join(timeout, true);
                lua_pushnumber(L, element.value);
                break;
            }
            case RecordObject::DOUBLE:
            {
                H5Element<double> element(&context, variable);
                element.join(timeout, true);
                lua_pushnumber(L, element.value);
                break;
            }
            case RecordObject::TIME8:     
            {
                H5Element<int64_t> element(&context, variable);
                element.join(timeout, true);
                lua_pushinteger(L, element.value);
                break;
            }
            case RecordObject::STRING:
            {
                H5Element<const char*> element(&context, variable);
                element.join(timeout, true);
                lua_pushlstring(L, element.value, element.size);
                break;
            }
            default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid data type specified <%d>", static_cast<int>(dtype));
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to read resource: %s", e.what());
        lua_pushnil(L);
    }

    /* Release Asset */
    if(asset) asset->releaseLuaObject();

    /* Return */
    return 1;
}

/*----------------------------------------------------------------------------
 * h5_open
 *----------------------------------------------------------------------------*/
int h5_open (lua_State *L)
{
    static const struct luaL_Reg h5_functions[] = {
        {"file",        H5File::luaCreate},
        {"dataset",     H5DatasetDevice::luaCreate},
        {"read",        h5_read},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, h5_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "ALL_ROWS", H5Coro::ALL_ROWS);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void inith5 (void)
{
    /* Initialize Modules */
    H5Coro::init(H5CORO_THREAD_POOL_SIZE);
    H5DArray::init();
    H5DatasetDevice::init();
    H5File::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_H5_LIBNAME, h5_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_H5_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_H5_LIBNAME, LIBID);
}

void deinith5 (void)
{
    H5Coro::deinit();
}
}
