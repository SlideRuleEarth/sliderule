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
                throw RunTimeException("invalid log level supplied: %s", lvl_str);
            }
        }

        /* Get Optional - As Record */
        bool as_record = getLuaBoolean(L, 3, true, false);

        /* Return Dispatch Object */
        return createLuaObject(L, new Logger(L, lvl, outq_name, MsgQ::CFG_DEPTH_STANDARD, as_record));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
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
                throw RunTimeException("invalid log level supplied: %s", lvl_str);
            }
        }

        /* Set Level */
        LogLib::setLevel(lua_obj->logid, lvl);

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring logger: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
