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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ProgressMessager: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ProgressMessager::init (void)
{
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(rec_type, NULL, sizeof(progress_message_t), rec_def, sizeof(rec_def) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", rec_type, rc);
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
        if(post_status <= 0)
        {
            throw RunTimeException(CRITICAL, "Failed to post progress message: %d", post_status);
        }

        /* Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error posting message: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
