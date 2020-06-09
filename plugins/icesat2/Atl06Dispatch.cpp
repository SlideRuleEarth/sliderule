/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Atl06Dispatch.h"
#include "Hdf5Atl03Handle.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl06Dispatch::LuaMetaName = "Atl06Dispatch";
const struct luaL_Reg Atl06Dispatch::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :atl06(<outq name>)
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl06Dispatch(L, outq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Dispatch::Atl06Dispatch (lua_State* L, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);

    outQ = new Publisher(outq_name);
    LocalLib::set(&stats, 0, sizeof(stats));
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl06Dispatch::~Atl06Dispatch(void)
{
    if(outQ) delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processRecord (RecordObject* record, okey_t key)
{
    double height = averageHeightStage(record, key);

    if(outQ->postCopy(&height, sizeof(height), SYS_TIMEOUT) > 0)
    {
        stats.post_success_cnt++;
        return true;
    }
    else
    {
        stats.post_dropped_cnt++;
        return false;
    }
}

/*----------------------------------------------------------------------------
 * averageHeightStage
 *----------------------------------------------------------------------------*/
double Atl06Dispatch::averageHeightStage (RecordObject* record, okey_t key)
{
    (void)key;

    Hdf5Atl03Handle::segment_t* segment = (Hdf5Atl03Handle::segment_t*)record->getRecordData();

    /* Count Execution Statistic */
    stats.avgheight_out_cnt++;

    /* Calculate Left Track Height */
    double height_l = 0.0;
    for(unsigned int ph = 0; ph < segment->num_photons[PRT_LEFT]; ph++)
    {
        height_l += (segment->photons[ph].height_y / segment->num_photons[PRT_LEFT]);
    }

    /* Calculate Right Track Height */
    double height_r = 0.0;
    for(unsigned int ph = segment->num_photons[PRT_LEFT]; ph < (segment->num_photons[PRT_LEFT] + segment->num_photons[PRT_RIGHT]); ph++)
    {
        height_r += (segment->photons[ph].height_y / segment->num_photons[PRT_RIGHT]);
    }

    /* Return Average Track Height */
    return (height_l + height_r) / 2.0;
}

/*----------------------------------------------------------------------------
 * luaStats
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Atl06Dispatch* lua_obj = (Atl06Dispatch*)getLuaSelf(L, 1);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "h5atl03_rec_cnt",     lua_obj->stats.h5atl03_rec_cnt);
        LuaEngine::setAttrInt(L, "avgheight_out_cnt",   lua_obj->stats.avgheight_out_cnt);
        LuaEngine::setAttrInt(L, "post_success_cnt",    lua_obj->stats.post_success_cnt);
        LuaEngine::setAttrInt(L, "post_dropped_cnt",    lua_obj->stats.post_dropped_cnt);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
