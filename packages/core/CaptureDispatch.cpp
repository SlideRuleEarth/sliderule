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

#include "CaptureDispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CaptureDispatch::LuaMetaName = "CaptureDispatch";
const struct luaL_Reg CaptureDispatch::LuaMetaTable[] = {
    {"capture",     luaCapture},
    {"clear",       luaClear},
    {"remove",      luaRemove},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<output stream name>)
 *----------------------------------------------------------------------------*/
int CaptureDispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1, true);

        /* Return Dispatch Object */
        return createLuaObject(L, new CaptureDispatch(L, outq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CaptureDispatch::CaptureDispatch (lua_State* L, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)

{
    outQ = NULL;
    if(outq_name) outQ = new Publisher(outq_name);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CaptureDispatch::~CaptureDispatch (void)
{
    if(outQ) delete outQ;
}

/*----------------------------------------------------------------------------
 * freeCaptureEntry
 *----------------------------------------------------------------------------*/
void CaptureDispatch::freeCaptureEntry (void* obj, void* parm)
{
    (void)parm;
    if(obj)
    {
        capture_t* entry = (capture_t*)obj;
        if(entry->field_name) delete [] entry->field_name;
        delete entry;
    }
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool CaptureDispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    capMut.lock();
    {
        /* Process Captures */
        for(int i = 0; i < captures.length(); i++)
        {
            capture_t* cap = captures[i];

            /* Filter on ID */
            if(cap->filter_id)
            {
                if(cap->id != record->getRecordId())
                {
                    continue;
                }
            }

            /* Capture */
            char valbuf[RecordObject::MAX_VAL_STR_SIZE];
            if(record->getValueText(record->getField(cap->field_name), valbuf))
            {
                /* Signal Blocking Command */
                if(cap->timeout > 0)
                {
                    cap->cond.signal();
                }

                /* Post Key:Value Pair */
                if(outQ)
                {
                    outQ->postString("%s:%s", cap->field_name, valbuf);
                }
            }
        }
    }
    capMut.unlock();

    return true;
}

/*----------------------------------------------------------------------------
 * luaCapture - :capture(<field name>, [<timeout>], [<id value>])
 *----------------------------------------------------------------------------*/
int CaptureDispatch::luaCapture (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CaptureDispatch* lua_obj = (CaptureDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool        filter      = false;
        const char* field_str   = getLuaString(L, 2);
        int         timeout     = (int)getLuaInteger(L, 3, true, SYS_TIMEOUT);
        long        id          = getLuaInteger(L, 4, true, 0, &filter);

        /* Allocate New Capture */
        capture_t* cap = new capture_t(filter, id, field_str, timeout);

        /* Add Capture to Captures */
        int cap_index;
        lua_obj->capMut.lock();
        {
            lua_obj->captures.add(cap);
            cap_index = lua_obj->captures.length() - 1;
        }
        lua_obj->capMut.unlock();

        /* Process Blocking Capture */
        if(cap->timeout > 0)
        {
            /* Perform Condition Wait */
            cap->cond.lock();
            status = cap->cond.wait(0, timeout);
            cap->cond.unlock();
            if(!status)
            {
                throw LuaException("timed out waiting to capture field");
            }

            /* Delete Capture */
            lua_obj->capMut.lock();
            {
                lua_obj->captures.remove(cap_index);
            }
            lua_obj->capMut.unlock();
        }

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error capturing: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaClear - :clear()
 *----------------------------------------------------------------------------*/
int CaptureDispatch::luaClear (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CaptureDispatch* lua_obj = (CaptureDispatch*)getLuaSelf(L, 1);

        /* Clear All Captures */
        lua_obj->capMut.lock();
        {
            lua_obj->captures.clear();
        }
        lua_obj->capMut.unlock();

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error removing all captures: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaRemove - :remove(<field name>)
 *----------------------------------------------------------------------------*/
int CaptureDispatch::luaRemove (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CaptureDispatch* lua_obj = (CaptureDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* field_str = getLuaString(L, 2);

        /* Remove Capture */
        lua_obj->capMut.lock();
        {
            for(int i = 0; i < lua_obj->captures.length(); i++)
            {
                if(StringLib::match(lua_obj->captures[i]->field_name, field_str))
                {
                    lua_obj->captures.remove(i);
                    status = true;
                }
            }
        }
        lua_obj->capMut.unlock();

    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error removing capture: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
