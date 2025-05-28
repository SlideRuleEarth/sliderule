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
#include "H5Coro.h"
#include "H5Column.h"
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* H5Column::OBJECT_TYPE = "H5Column";
const char* H5Column::LUA_META_NAME = "H5Column";
const struct luaL_Reg H5Column::LUA_META_TABLE[] = {
    {"timeout",     luaTimeout},
    {"at",          luaIndex},
    {"sum",         luaSum},
    {"mean",        luaMean},
    {"median",      luaMedian},
    {"mode",        luaMode},
    {"unique",      luaUnique},
    {NULL,          NULL}
};

/******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Column::H5Column (lua_State* L, H5Coro::Future* _future):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    timeoutMs(SystemConfig::settings().publishTimeoutMs.value),
    future(_future),
    column(NULL)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Column::~H5Column (void)
{
    delete future;
    delete column;
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
void H5Column::join (int timeout_ms)
{
    if(!column)
    {
        const H5Coro::Future::rc_t rc = future->wait(timeout_ms);
        if(rc == H5Coro::Future::COMPLETE)
        {
            switch(future->info.datatype)
            {
                case RecordObject::INT8:   column = new FieldColumn<int8_t>  (future->info.data, future->info.datasize); break;
                case RecordObject::INT16:  column = new FieldColumn<int16_t> (future->info.data, future->info.datasize); break;
                case RecordObject::INT32:  column = new FieldColumn<int32_t> (future->info.data, future->info.datasize); break;
                case RecordObject::INT64:  column = new FieldColumn<int64_t> (future->info.data, future->info.datasize); break;
                case RecordObject::UINT8:  column = new FieldColumn<uint8_t> (future->info.data, future->info.datasize); break;
                case RecordObject::UINT16: column = new FieldColumn<uint16_t>(future->info.data, future->info.datasize); break;
                case RecordObject::UINT32: column = new FieldColumn<uint32_t>(future->info.data, future->info.datasize); break;
                case RecordObject::UINT64: column = new FieldColumn<uint64_t>(future->info.data, future->info.datasize); break;
                case RecordObject::FLOAT:  column = new FieldColumn<float>   (future->info.data, future->info.datasize); break;
                case RecordObject::DOUBLE: column = new FieldColumn<double>  (future->info.data, future->info.datasize); break;
                case RecordObject::TIME8:  column = new FieldColumn<time8_t> (future->info.data, future->info.datasize); break;
                case RecordObject::BOOL:   column = new FieldColumn<bool>    (future->info.data, future->info.datasize); break;
                default: throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to convert type %d into a column", static_cast<int>(future->info.datatype));
            }
        }
        else
        {
            throw RunTimeException(ERROR, RTE_TIMEOUT, "data unavailable: %d", static_cast<int>(rc));
        }
    }
}

/*----------------------------------------------------------------------------
 * luaTimeout
 *----------------------------------------------------------------------------*/
int H5Column::luaTimeout (lua_State* L)
{
    bool status = true;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout_ms = getLuaInteger(L, 2);

        // check timeout
        if(timeout_ms <= 0 || timeout_ms > (SystemConfig::settings().requestTimeoutSec.value * 1000))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid timeout: %d ms", timeout_ms);
        }

        // set timeout
        lua_obj->timeoutMs = timeout_ms;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaIndex
 *----------------------------------------------------------------------------*/
int H5Column::luaIndex (lua_State* L)
{
    bool status = true;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const long index = getLuaInteger(L, 2) - 1; // lua indexing starts at 1, convert to c indexing that starts at 0
        const int timeout = getLuaInteger(L, 3, true, lua_obj->timeoutMs);

        // check index
        if(index < 0) throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid index: %ld", index + 1);

        // check future
        lua_obj->join(timeout);

        // handle field access
        return lua_obj->column->toLua(L, index);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSum
 *----------------------------------------------------------------------------*/
int H5Column::luaSum (lua_State* L)
{
    bool status = true;
    int num_ret = 1;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout = getLuaInteger(L, 2, true, lua_obj->timeoutMs);

        // check future
        lua_obj->join(timeout);

        // calculate sum
        double result = lua_obj->column->sum();
        lua_pushnumber(L, result);
        num_ret++;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaMean
 *----------------------------------------------------------------------------*/
int H5Column::luaMean (lua_State* L)
{
    bool status = true;
    int num_ret = 1;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout = getLuaInteger(L, 2, true, lua_obj->timeoutMs);

        // check future
        lua_obj->join(timeout);

        // calculate average (mean)
        double result = lua_obj->column->mean();
        lua_pushnumber(L, result);
        num_ret++;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaMedian
 *----------------------------------------------------------------------------*/
int H5Column::luaMedian (lua_State* L)
{
    bool status = true;
    int num_ret = 1;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout = getLuaInteger(L, 2, true, lua_obj->timeoutMs);

        // check future
        lua_obj->join(timeout);

        // calculate average (median)
        double result = lua_obj->column->median();
        lua_pushnumber(L, result);
        num_ret++;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaMode
 *----------------------------------------------------------------------------*/
int H5Column::luaMode (lua_State* L)
{
    bool status = true;
    int num_ret = 1;

    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout = getLuaInteger(L, 2, true, lua_obj->timeoutMs);

        // check future
        lua_obj->join(timeout);

        // calculate average (mode)
        double result = lua_obj->column->mode();
        lua_pushnumber(L, result);
        num_ret++;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        status = false;
    }

    // return status
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaUnique
 *----------------------------------------------------------------------------*/
int H5Column::luaUnique (lua_State* L)
{
    try
    {
        // get parameters
        H5Column* lua_obj = dynamic_cast<H5Column*>(getLuaSelf(L, 1));
        const int timeout = getLuaInteger(L, 2, true, lua_obj->timeoutMs);

        // check future
        lua_obj->join(timeout);

        // calculate unique values
        FieldUntypedColumn::unique_map_t result = lua_obj->column->unique();

        // return results in a table
        lua_newtable(L);
        for(const auto& [key, value] : result)
        {
            lua_pushinteger(L, key);
            lua_pushinteger(L, value);
            lua_settable(L, -3);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in %s: %s\n", __FUNCTION__, e.what());
        lua_pushnil(L);
    }

    // return
    return 1;
}