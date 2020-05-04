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

#include "PublisherDispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* PublisherDispatch::LuaMetaName = "PublisherDispatch";
const struct luaL_Reg PublisherDispatch::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int PublisherDispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* recq_name = getLuaString(L, 1);

        /* Create Record Monitor */
        return createLuaObject(L, new PublisherDispatch(L, recq_name));
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
PublisherDispatch::PublisherDispatch(lua_State* L, const char* recq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(recq_name);

    pubQ = new Publisher(recq_name);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PublisherDispatch::~PublisherDispatch(void)
{
    delete pubQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool PublisherDispatch::processRecord(RecordObject* record, okey_t key)
{
    (void)key;
    unsigned char* buffer; // reference to serial buffer
    int size = record->serialize(&buffer, RecordObject::REFERENCE);
    if(size > 0)    return (pubQ->postCopy(buffer, size) > 0);
    else            return false;
}
