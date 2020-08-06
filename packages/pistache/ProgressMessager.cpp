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

#include "ProgressMessager.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ProgressMessager::rec_type = "progressrec";
RecordObject::fieldDef_t ProgressMessager::rec_def[] =
{
    {"message", RecordObject::STRING, offsetof(progress_message_t, message),  MAX_MESSAGE_SIZE, NULL, NATIVE_FLAGS},
};
int ProgressMessager::rec_elem = sizeof(ProgressMessager::rec_def) / sizeof(RecordObject::fieldDef_t);

const char* ProgressMessager::OBJECT_TYPE = "ProgressMessager";
const char* ProgressMessager::LuaMetaName = "ProgressMessager";
const struct luaL_Reg ProgressMessager::LuaMetaTable[] = {
    {"post",        luaPost},
    {NULL,          NULL}
};

/******************************************************************************
 * PROGRESS MESSAGE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<rspq_name>)
 *----------------------------------------------------------------------------*/
int ProgressMessager::luaCreate (lua_State* L)
{
    try
    {
        /* Get Response Queue */
        const char* rspq_name = getLuaString(L, 1);

        /* Return Dispatch Object */
        return createLuaObject(L, new ProgressMessager(L, rspq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating ProgressMessager: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ProgressMessager::init (void)
{
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(rec_type, NULL, sizeof(progress_message_t), rec_def, sizeof(rec_def) / sizeof(RecordObject::fieldDef_t), 16);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", rec_type, rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ProgressMessager::ProgressMessager (lua_State* L, const char* rspq_name):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(rspq_name);
    rspQ = new Publisher(rspq_name);
    record = new RecordObject(rec_type);
    progressMessage = (progress_message_t*)record->getRecordData();

}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ProgressMessager::~ProgressMessager (void)
{
    delete rspQ;
    delete record;
}

/*----------------------------------------------------------------------------
 * luaPost - :post(<message>)
 *----------------------------------------------------------------------------*/
int ProgressMessager::luaPost (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        ProgressMessager* lua_obj = (ProgressMessager*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* message = getLuaString(L, 2);

        /* Create Message */
        StringLib::copy(lua_obj->progressMessage->message, message, MAX_MESSAGE_SIZE);

        /* Post Message */
        uint8_t* rec_buf = NULL;
        int rec_bytes = lua_obj->record->serialize(&rec_buf, RecordObject::REFERENCE);
        int post_status = lua_obj->rspQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT);
        if(post_status != MsgQ::STATE_OKAY)
        {
            throw LuaException("Failed to post progress message: %d", post_status);
        }
        
        /* Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error posting message: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
