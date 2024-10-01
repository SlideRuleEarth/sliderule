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
#include "BathyFields.h"
#include "BathyKd.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyKd::OBJECT_TYPE = "BathyKd";
const char* BathyKd::LUA_META_NAME = "BathyKd";
const struct luaL_Reg BathyKd::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int BathyKd::luaCreate (lua_State* L)
{
    BathyFields* parms = NULL;
    H5Coro::Context* _context = NULL;
    try
    {
        parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        const char* resource_kd = getLuaString(L, 2);
        if(!parms->uncertainty.assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to open Kd resource, no asset provided");
        _context = new H5Coro::Context(parms->uncertainty.assetKd, resource_kd);
        parms->releaseLuaObject();
        return createLuaObject(L, new BathyKd(L, _context));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyKd: %s", e.what());
        if(parms) parms->releaseLuaObject();
        delete _context;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
void BathyKd::join (int timeout)
{
    array->join(timeout, true);
}

/*----------------------------------------------------------------------------
 * getKd
 *----------------------------------------------------------------------------*/
double BathyKd::getKd (double lon, double lat)
{
    /* get y offset */
    const double degrees_of_latitude = lat + 90.0;
    const double latitude_pixels = degrees_of_latitude * 24.0;
    const int32_t y = static_cast<int32_t>(latitude_pixels);

    /* get x offset */
    const double degrees_of_longitude = lon + 180.0;
    const double longitude_pixels = degrees_of_longitude * 24.0;
    const int32_t x = static_cast<int32_t>(longitude_pixels);

    /* calculate total offset */
    if(y < 0 || y >= 4320 || x < 0 || x >= 8640)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid Kd coordinates: %d, %d | %lf, %lf", y, x, degrees_of_latitude, degrees_of_longitude);
    }
    const long offset = (x * 4320) + y;

    /* get kd */
    return static_cast<double>((*array)[offset]) * 0.0002;

}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyKd::BathyKd (lua_State* L, H5Coro::Context* _context):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    context(_context)
{
    assert(context);
    array = new H5Array<int16_t>(context, "Kd_490", H5Coro::ALL_COLS, 0, H5Coro::ALL_ROWS);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyKd::~BathyKd (void)
{
    delete array;
    delete context;
}
