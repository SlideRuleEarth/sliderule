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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "core.h"
#include "arcticdem.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_STAT_TILES_READ "read"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArcticDEMReader::tileRecType = "tilerec";
const RecordObject::fieldDef_t ArcticDEMReader::tileRecDef[] = {
    {"valid",   RecordObject::INT8,     offsetof(tile_t, valid),    1,  NULL, NATIVE_FLAGS},
};

const char* ArcticDEMReader::OBJECT_TYPE = "ArcticDEMReader";
const char* ArcticDEMReader::LuaMetaName = "ArcticDEMReader";
const struct luaL_Reg ArcticDEMReader::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

const ArcticDEMReader::dem_parms_t ArcticDEMReader::DefaultParms = {};

/******************************************************************************
 * ARTICDEM READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>)
 *----------------------------------------------------------------------------*/
int ArcticDEMReader::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        Asset* asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        dem_parms_t* parms = getLuaDEMParms(L, 4);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new ArcticDEMReader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArcticDEMReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArcticDEMReader::init (void)
{
    RecordObject::recordDefErr_t tile_rc = RecordObject::defineRecord(tileRecType, NULL, sizeof(tile_t), tileRecDef, sizeof(tileRecDef) / sizeof(RecordObject::fieldDef_t));
    if(tile_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", tileRecType, tile_rc);
    }
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArcticDEMReader::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArcticDEMReader::ArcticDEMReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, dem_parms_t* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.tiles_read = 0;

    /* Initialize Reader */
    active = true;
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArcticDEMReader::~ArcticDEMReader (void)
{
    active = false;
    delete readerPid;
    delete outQ;
    delete parms;
    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* ArcticDEMReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    ArcticDEMReader* reader = (ArcticDEMReader*)parm;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "articdem_reader", "{\"asset\":\"%s\", \"resource\":\"%s\"}", reader->asset->getName(), reader->resource);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    /* Update Statistics */
    reader->stats.tiles_read += 1;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * getLuaDEMParms
 *----------------------------------------------------------------------------*/
ArcticDEMReader::dem_parms_t* ArcticDEMReader::getLuaDEMParms (lua_State* L, int index)
{
    dem_parms_t* parms = new dem_parms_t; // freed in ATL03Reader and ATL06Dispatch destructor
    *parms = DefaultParms; // initialize with defaults

    if(lua_type(L, index) == LUA_TTABLE)
    {
        try
        {
            /* Get Polygon */
            lua_getfield(L, index, LUA_PARM_POLYGON);
            if(lua_istable(L, -1))
            {
                int num_points = lua_rawlen(L, -1);
                for(int i = 0; i < num_points; i++)
                {
                    /* Get coordinate table */
                    lua_rawgeti(L, -1, i+1);
                    if(lua_istable(L, -1))
                    {
                        MathLib::coord_t coord;

                        /* Get longitude entry */
                        lua_getfield(L, -1, LUA_PARM_LONGITUDE);
                        coord.lon = LuaObject::getLuaFloat(L, -1);
                        lua_pop(L, 1);

                        /* Get latitude entry */
                        lua_getfield(L, -1, LUA_PARM_LATITUDE);
                        coord.lat = LuaObject::getLuaFloat(L, -1);
                        lua_pop(L, 1);

                        /* Add Coordinate */
                        parms->polygon.add(coord);
                        mlog(DEBUG, "Setting %s to %d points", LUA_PARM_POLYGON, (int)parms->polygon.length());
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
        catch(const RunTimeException& e)
        {
            delete parms; // free allocated parms since it won't be owned by anything else
            throw; // rethrow exception
        }
    }

    return parms;
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int ArcticDEMReader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    ArcticDEMReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (ArcticDEMReader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_TILES_READ, lua_obj->stats.tiles_read);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
