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

#include "LimitDispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LimitDispatch::LuaMetaName = "LimitDispatch";
const struct luaL_Reg LimitDispatch::LuaMetaTable[] = {
    {"setloglvl",   luaSetLogLevel},
    {"gmtdisplay",  luaGMTDisplay},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :limit(<field>, <value of id to filter on>, <min>, <max>)
 *----------------------------------------------------------------------------*/
int LimitDispatch::luaCreate (lua_State* L)
{
    try
    {
        LimitRecord::limit_t rec;
        LocalLib::set(&rec, 0, sizeof(LimitRecord::limit_t));

        /* Initialize Limit Record with Parameters */
        StringLib::format(rec.field_name, LimitRecord::MAX_FIELD_NAME_SIZE, "%s", getLuaString(L, 1));
        rec.id    = getLuaInteger(L, 2, true, 0, &rec.filter_id);
        rec.d_min = getLuaFloat(L, 3, true, 0.0, &rec.limit_min);
        rec.d_max = getLuaFloat(L, 4, true, 0.0, &rec.limit_max);

        /* Get Limit Dispatch Parameters */
        const char* deepq  = getLuaString(L, 5, true, NULL); // for posting the offending record
        const char* limitq = getLuaString(L, 6, true, NULL); // for posting the limit record

        /* Create Record Monitor */
        return createLuaObject(L, new LimitDispatch(L, rec, deepq, limitq));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LimitDispatch::LimitDispatch (lua_State* L, LimitRecord::limit_t _limit, const char* deepq_name, const char* limitq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    LimitRecord::defineRecord(LimitRecord::rec_type, "TYPE", sizeof(LimitRecord::limit_t), LimitRecord::rec_def, LimitRecord::rec_elem, 32);

    limit = _limit;
    logLevel = ERROR;
    inError = false;

    limitQ = NULL;
    if(limitq_name)
    {
        limitQ = new Publisher(limitq_name);
    }

    deepQ = NULL;
    if(deepq_name)
    {
        deepQ = new Publisher(deepq_name);
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
LimitDispatch::~LimitDispatch(void)
{
    if(limitQ) delete limitQ;
    if(deepQ) delete deepQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool LimitDispatch::processRecord (RecordObject* record, okey_t key)
{
    bool status = true;
    bool enabled = true;

    /* Filter on ID */
    if(limit.filter_id)
    {
        if(limit.id != record->getRecordId())
        {
            enabled = false;
        }
    }

    /* Limit Check */
    if(enabled)
    {
        RecordObject::field_t field = record->getField(limit.field_name);
        if(field.type != RecordObject::INVALID_FIELD)
        {
            inError = false;
            double val = record->getValueReal(field);
            if( ((limit.limit_min) && (limit.d_min > val)) ||
                ((limit.limit_max) && (limit.d_max < val)) )
            {
                /* Create Limit Violation */
                LimitRecord violation(limit);
                violation.limit->d_val = val;
                StringLib::format(violation.limit->record_name, LimitRecord::MAX_RECORD_NAME_SIZE, "%s", record->getRecordType());

                /* Log Error Message */
                if(gmtDisplay)
                {
                    TimeLib::gmt_time_t index_time = TimeLib::gps2gmttime(key);
                    mlog(logLevel, "Limit violation for %s - %s(%ld): %lf violates %s: [%lf, %lf] at %d:%d:%d:%d:%d:%d",
                            limit.field_name, violation.limit->record_name, record->getRecordId(),
                            violation.limit->d_val, ObjectType, limit.d_min, limit.d_max,
                            index_time.year, index_time.day, index_time.hour, index_time.minute, index_time.second, index_time.millisecond);
                }
                else
                {
                    mlog(logLevel, "Limit violation for %s - %s(%ld): %lf violates %s: [%lf, %lf]",
                            limit.field_name, violation.limit->record_name, record->getRecordId(),
                            violation.limit->d_val, ObjectType, limit.d_min, limit.d_max);
                }

                /* Post Violation to Limit Q */
                if(limitQ)
                {
                    unsigned char* buffer; // reference to serial buffer
                    int size = violation.serialize(&buffer, RecordObject::REFERENCE);
                    if(size > 0) limitQ->postCopy(buffer, size);
                }

                /* Post Record to Deep Copy Q */
                if(deepQ)
                {
                    unsigned char* buffer; // reference to serial buffer
                    int size = record->serialize(&buffer, RecordObject::REFERENCE);
                    if(size > 0) deepQ->postCopy(buffer, size);
                }
            }
        }
        else if(inError == false)
        {
            inError = true;
            mlog(WARNING, "Failed to find field %s in record %s", limit.field_name, record->getRecordType());
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * setLogLevelCmd - :setloglvl(DEBUG|INFO|WARNING|ERROR|CRITICAL)
 *----------------------------------------------------------------------------*/
int LimitDispatch::luaSetLogLevel(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LimitDispatch* lua_obj = (LimitDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        event_level_t lvl = (event_level_t)getLuaInteger(L, 2);

        /* Set Level */
        lua_obj->logLevel = lvl;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error setting level: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaGMTDisplay - :gmtdisplay(true|false)
 *----------------------------------------------------------------------------*/
int LimitDispatch::luaGMTDisplay(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LimitDispatch* lua_obj = (LimitDispatch*)getLuaSelf(L, 1);

        /* Configure Display */
        lua_obj->gmtDisplay = getLuaBoolean(L, 2, false, false, &status);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring GMT display: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
