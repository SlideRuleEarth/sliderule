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

#include "Logger.h"
#include "LogLib.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Logger::recType = "logrec";
RecordObject::fieldDef_t Logger::recDef[] =
{
    {"level",   RecordObject::INT32,    offsetof(log_message_t, message),  1,                           NULL, NATIVE_FLAGS},
    {"message", RecordObject::STRING,   offsetof(log_message_t, message),  LogLib::MAX_LOG_ENTRY_SIZE,  NULL, NATIVE_FLAGS}
};

const char* Logger::OBJECT_TYPE = "Logger";
const char* Logger::LuaMetaName = "Logger";
const struct luaL_Reg Logger::LuaMetaTable[] = {
    {"config",      luaConfig},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Logger::init (void)
{
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(recType, NULL, sizeof(log_message_t), recDef, sizeof(recDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", recType, rc);
    }
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<output stream name>, <level>, [<as_record>])
 *----------------------------------------------------------------------------*/
int Logger::luaCreate (lua_State* L)
{
    try
    {
        /* Get Ouput Stream */
        const char* outq_name = getLuaString(L, 1);

        /* Get Level */
        log_lvl_t lvl;
        if(lua_isinteger(L, 2))
        {
            lvl = (log_lvl_t)getLuaInteger(L, 2);
        }
        else
        {
            const char* lvl_str = getLuaString(L, 2);
            if(!LogLib::str2lvl(lvl_str, &lvl))
            {
                throw LuaException("invalid log level supplied: %s", lvl_str);
            }
        }

        /* Get Optional - As Record */
        bool as_record = getLuaBoolean(L, 3, true, false);

        /* Return Dispatch Object */
        return createLuaObject(L, new Logger(L, lvl, outq_name, MsgQ::CFG_DEPTH_STANDARD, as_record));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * logHandler
 *----------------------------------------------------------------------------*/
int Logger::logHandler(const char* msg, int size, void* parm)
{
    Logger* logger = (Logger*)parm;
    return logger->outq->postCopy(msg, size);
}

/*----------------------------------------------------------------------------
 * recHandler
 *----------------------------------------------------------------------------*/
int Logger::recHandler(const char* msg, int size, void* parm)
{
    Logger* logger = (Logger*)parm;

    /* Copy Into Record */
    StringLib::copy(logger->logmsg->message, msg, size);

    /* Post Record */
    uint8_t* rec_buf = NULL;
    int rec_bytes = logger->record->serialize(&rec_buf, RecordObject::REFERENCE);
    return logger->outq->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Logger::Logger(lua_State* L, log_lvl_t _level, const char* outq_name, int qdepth, bool as_record):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);

    outq = new Publisher(outq_name, NULL, qdepth);
    if(as_record)
    {
        logid = LogLib::createLog(_level, recHandler, this);
        record = new RecordObject(recType);
        logmsg = (log_message_t*)record->getRecordData();
        logmsg->level = _level;
    }
    else
    {
        logid = LogLib::createLog(_level, logHandler, this);
        record = NULL;
        logmsg = NULL;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Logger::~Logger(void)
{
    LogLib::deleteLog(logid);
    delete outq;
    if(record) delete record;
}

/*----------------------------------------------------------------------------
 * luaConfig - :config(<lvl>)
 *----------------------------------------------------------------------------*/
int Logger::luaConfig (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Logger* lua_obj = (Logger*)getLuaSelf(L, 1);

        /* Get Level */
        log_lvl_t lvl;
        if(lua_isinteger(L, 2))
        {
            lvl = (log_lvl_t)getLuaInteger(L, 2);
        }
        else
        {
            const char* lvl_str = getLuaString(L, 2);
            if(!LogLib::str2lvl(lvl_str, &lvl))
            {
                throw LuaException("invalid log level supplied: %s", lvl_str);
            }
        }

        /* Set Level */
        LogLib::setLevel(lua_obj->logid, lvl);

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring logger: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
