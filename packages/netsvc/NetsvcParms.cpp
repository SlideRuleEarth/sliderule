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

#include "core.h"
#include "geo.h"
#include "NetsvcParms.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* NetsvcParms::POLYGON        = "poly";
const char* NetsvcParms::RASTER         = "raster";
const char* NetsvcParms::LATITUDE       = "lat";
const char* NetsvcParms::LONGITUDE      = "lon";
const char* NetsvcParms::RQST_TIMEOUT   = "rqst-timeout";
const char* NetsvcParms::NODE_TIMEOUT   = "node-timeout";
const char* NetsvcParms::READ_TIMEOUT   = "read-timeout";
const char* NetsvcParms::GLOBAL_TIMEOUT = "timeout";

const char* NetsvcParms::OBJECT_TYPE = "NetsvcParms";
const char* NetsvcParms::LuaMetaName = "NetsvcParms";
const struct luaL_Reg NetsvcParms::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int NetsvcParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Network service parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new NetsvcParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
NetsvcParms::NetsvcParms(lua_State* L, int index):
    LuaObject                   (L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    raster                      (NULL),
    rqst_timeout                (DEFAULT_RQST_TIMEOUT),
    node_timeout                (DEFAULT_NODE_TIMEOUT),
    read_timeout                (DEFAULT_READ_TIMEOUT)
{
    bool provided = false;

    try
    {
        /* Polygon */
        lua_getfield(L, index, NetsvcParms::POLYGON);
        get_lua_polygon(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d points", NetsvcParms::POLYGON, (int)polygon.length());
        lua_pop(L, 1);

        /* Raster */
        lua_getfield(L, index, NetsvcParms::RASTER);
        get_lua_raster(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s file for use", NetsvcParms::RASTER);
        lua_pop(L, 1);

        /* Global Timeout */
        lua_getfield(L, index, NetsvcParms::GLOBAL_TIMEOUT);
        int global_timeout = LuaObject::getLuaInteger(L, -1, true, 0, &provided);
        if(provided)
        {
            rqst_timeout = global_timeout;
            node_timeout = global_timeout;
            read_timeout = global_timeout;
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::RQST_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::NODE_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::READ_TIMEOUT, global_timeout);
        }
        lua_pop(L, 1);

        /* Request Timeout */
        lua_getfield(L, index, NetsvcParms::RQST_TIMEOUT);
        rqst_timeout = LuaObject::getLuaInteger(L, -1, true, rqst_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::RQST_TIMEOUT, rqst_timeout);
        lua_pop(L, 1);

        /* Node Timeout */
        lua_getfield(L, index, NetsvcParms::NODE_TIMEOUT);
        node_timeout = LuaObject::getLuaInteger(L, -1, true, node_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::NODE_TIMEOUT, node_timeout);
        lua_pop(L, 1);

        /* Read Timeout */
        lua_getfield(L, index, NetsvcParms::READ_TIMEOUT);
        read_timeout = LuaObject::getLuaInteger(L, -1, true, read_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::READ_TIMEOUT, read_timeout);
        lua_pop(L, 1);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw; // rethrow exception
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
NetsvcParms::~NetsvcParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void NetsvcParms::cleanup (void)
{
    if(raster) delete raster;
}

/*----------------------------------------------------------------------------
 * get_lua_polygon
 *----------------------------------------------------------------------------*/
void NetsvcParms::get_lua_polygon (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        /* Get Number of Points in Polygon */
        int num_points = lua_rawlen(L, index);

        /* Iterate through each coordinate */
        for(int i = 0; i < num_points; i++)
        {
            /* Get coordinate table */
            lua_rawgeti(L, index, i+1);
            if(lua_istable(L, index))
            {
                MathLib::coord_t coord;

                /* Get longitude entry */
                lua_getfield(L, index, NetsvcParms::LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get latitude entry */
                lua_getfield(L, index, NetsvcParms::LATITUDE);
                coord.lat = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Add Coordinate */
                polygon.add(coord);
                if(provided) *provided = true;
            }
            lua_pop(L, 1);
        }
    }
}

/*----------------------------------------------------------------------------
 * get_lua_raster
 *----------------------------------------------------------------------------*/
void NetsvcParms::get_lua_raster (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        try
        {
            raster = GeoJsonRaster::create(L, index);
            *provided = true;
        }
        catch(const RunTimeException& e)
        {
            mlog(e.level(), "Error creating GeoJsonRaster file: %s", e.what());
        }
    }
}
