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
                    mlog(logLevel, "Limit violation for %s - %s(%ld): %lf violates %s: [%lf, %lf] at %d:%d:%d:%d:%d:%d\n",
                            limit.field_name, violation.limit->record_name, record->getRecordId(),
                            violation.limit->d_val, ObjectType, limit.d_min, limit.d_max,
                            index_time.year, index_time.day, index_time.hour, index_time.minute, index_time.second, index_time.millisecond);
                }
                else
                {
                    mlog(logLevel, "Limit violation for %s - %s(%ld): %lf violates %s: [%lf, %lf]\n",
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
            mlog(WARNING, "Failed to find field %s in record %s\n", limit.field_name, record->getRecordType());
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * setLogLevelCmd - :setloglvl("RAW"|"IGNORE"|"DEBUG"|"INFO"|"WARNING"|"ERROR"|"CRITICAL")
 *----------------------------------------------------------------------------*/
int LimitDispatch::luaSetLogLevel(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LimitDispatch* lua_obj = (LimitDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* lvl_str = getLuaString(L, 2);

        /* Convert String to Level */
        log_lvl_t lvl;
        if(LogLib::str2lvl(lvl_str, &lvl) == false)
        {
            throw LuaException("invalid level");
        }

        /* Set Level */
        lua_obj->logLevel = lvl;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting level: %s\n", e.errmsg);
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
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring GMT display: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
