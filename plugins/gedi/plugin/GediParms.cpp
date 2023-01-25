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
#include "GediParms.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GediParms::POLYGON          = "poly";
const char* GediParms::RASTER           = "raster";
const char* GediParms::BEAM             = "beam";
const char* GediParms::DEGRADE_FLAG     = "degrade_flag";
const char* GediParms::L2_QUALITY_FLAG  = "l2_quality_flag";
const char* GediParms::L4_QUALITY_FLAG  = "l4_quality_flag";
const char* GediParms::SURFACE_FLAG     = "surface_flag";

const uint8_t GediParms::BEAM_NUMBER[NUM_BEAMS] = {0, 1, 2, 3, 5, 6, 8, 11};

const char* GediParms::OBJECT_TYPE = "GediParms";
const char* GediParms::LuaMetaName = "GediParms";
const struct luaL_Reg GediParms::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int GediParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Gedi parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new GediParms(L, 1));
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
GediParms::GediParms(lua_State* L, int index):
    LuaObject                   (L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    raster                      (NULL),
    beam                        (ALL_BEAMS),
    degrade_filter              (DEGRADE_UNFILTERED),
    l2_quality_filter           (L2QLTY_UNFILTERED),
    l4_quality_filter           (L4QLTY_UNFILTERED),
    surface_filter              (SURFACE_UNFILTERED)
{
    bool provided = false;

    try
    {
        /* Polygon */
        lua_getfield(L, index, GediParms::POLYGON);
        get_lua_polygon(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d points", GediParms::POLYGON, (int)polygon.length());
        lua_pop(L, 1);

        /* Raster */
        lua_getfield(L, index, GediParms::RASTER);
        get_lua_raster(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s file for use", GediParms::RASTER);
        lua_pop(L, 1);

        /* Beam */
        lua_getfield(L, index, GediParms::BEAM);
        beam = LuaObject::getLuaInteger(L, -1, true, beam, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::BEAM, beam);
        lua_pop(L, 1);

        /* Global Timeout */
        lua_getfield(L, index, GediParms::GLOBAL_TIMEOUT);
        int global_timeout = LuaObject::getLuaInteger(L, -1, true, 0, &provided);
        if(provided)
        {
            rqst_timeout = global_timeout;
            node_timeout = global_timeout;
            read_timeout = global_timeout;
            mlog(DEBUG, "Setting %s to %d", GediParms::RQST_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", GediParms::NODE_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", GediParms::READ_TIMEOUT, global_timeout);
        }
        lua_pop(L, 1);

        /* Request Timeout */
        lua_getfield(L, index, GediParms::RQST_TIMEOUT);
        rqst_timeout = LuaObject::getLuaInteger(L, -1, true, rqst_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::RQST_TIMEOUT, rqst_timeout);
        lua_pop(L, 1);

        /* Node Timeout */
        lua_getfield(L, index, GediParms::NODE_TIMEOUT);
        node_timeout = LuaObject::getLuaInteger(L, -1, true, node_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::NODE_TIMEOUT, node_timeout);
        lua_pop(L, 1);

        /* Read Timeout */
        lua_getfield(L, index, GediParms::READ_TIMEOUT);
        read_timeout = LuaObject::getLuaInteger(L, -1, true, read_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::READ_TIMEOUT, read_timeout);
        lua_pop(L, 1);

        /* Degrade Flag */
        lua_getfield(L, index, GediParms::DEGRADE_FLAG);
        degrade_filter = LuaObject::getLuaInteger(L, -1, true, degrade_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::DEGRADE_FLAG, degrade_filter);
        lua_pop(L, 1);

        /* L2 Quality Flag */
        lua_getfield(L, index, GediParms::L2_QUALITY_FLAG);
        l2_quality_filter = LuaObject::getLuaInteger(L, -1, true, l2_quality_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::L2_QUALITY_FLAG, l2_quality_filter);
        lua_pop(L, 1);

        /* L4 Quality Flag */
        lua_getfield(L, index, GediParms::L4_QUALITY_FLAG);
        l4_quality_filter = LuaObject::getLuaInteger(L, -1, true, l4_quality_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::L4_QUALITY_FLAG, l4_quality_filter);
        lua_pop(L, 1);

        /* Surface Flag */
        lua_getfield(L, index, GediParms::SURFACE_FLAG);
        surface_filter = LuaObject::getLuaInteger(L, -1, true, surface_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::SURFACE_FLAG, surface_filter);
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
GediParms::~GediParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void GediParms::cleanup (void)
{
    if(raster) delete raster;
}

/*----------------------------------------------------------------------------
 * get_lua_polygon
 *----------------------------------------------------------------------------*/
void GediParms::get_lua_polygon (lua_State* L, int index, bool* provided)
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
                lua_getfield(L, index, GediParms::LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get latitude entry */
                lua_getfield(L, index, GediParms::LATITUDE);
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
void GediParms::get_lua_raster (lua_State* L, int index, bool* provided)
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
