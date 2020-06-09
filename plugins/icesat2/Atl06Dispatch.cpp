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
    (void)key;

    Hdf5Atl03Handle::segment_t* segment = (Hdf5Atl03Handle::segment_t*)record->getValueText(record->getField("DATA"));

    double height = 0.0;
    for(unsigned int ph = 0; ph < segment->num_photons[PRT_LEFT]; ph++)
    {
        height += (segment->photons[ph].height_y / segment->num_photons[PRT_LEFT]);
    }

    if(outQ->postCopy(&height, sizeof(height), SYS_TIMEOUT) > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}
