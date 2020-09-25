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

#include "LuaObject.h"
#include "LuaEngine.h"
#include "LogLib.h"
#include "TraceLib.h"
#include "StringLib.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaObject::BASE_OBJECT_TYPE = "LuaObject";
const char* LuaException::ERROR_TYPE = "LuaException";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaException::LuaException(const char* _errmsg, ...):
    std::runtime_error(ERROR_TYPE)
{
    errmsg[0] = '\0';

    va_list args;
    va_start(args, _errmsg);
    int vlen = vsnprintf(errmsg, ERROR_MSG_LEN - 1, _errmsg, args);
    int msglen = MIN(vlen, ERROR_MSG_LEN - 1);
    va_end(args);

    if (msglen >= 0) errmsg[msglen] = '\0';
}

/*----------------------------------------------------------------------------
 * getType
 *----------------------------------------------------------------------------*/
const char* LuaObject::getType (void)
{
    if(ObjectType)  return ObjectType;
    else            return "<untyped>";
}

/*----------------------------------------------------------------------------
 * getName
 *----------------------------------------------------------------------------*/
const char* LuaObject::getName (void)
{
    if(ObjectName)  return ObjectName;
    else            return "<unnamed>";
}

/*----------------------------------------------------------------------------
 * getTraceId
 *----------------------------------------------------------------------------*/
uint32_t LuaObject::getTraceId (void)
{
    return traceId;
}

/*----------------------------------------------------------------------------
 * getLuaNumParms
 *----------------------------------------------------------------------------*/
int LuaObject::getLuaNumParms (lua_State* L)
{
    return lua_gettop(L);
}

/*----------------------------------------------------------------------------
 * getLuaInteger
 *----------------------------------------------------------------------------*/
long LuaObject::getLuaInteger (lua_State* L, int parm, bool optional, long dfltval, bool* provided)
{
    if(provided) *provided = false;

    if(lua_isinteger(L, parm))
    {
        if(provided) *provided = true;
        return lua_tointeger(L, parm);
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw LuaException("must supply an integer for parameter #%d", parm);
    }
}

/*----------------------------------------------------------------------------
 * getLuaFloat
 *----------------------------------------------------------------------------*/
double LuaObject::getLuaFloat (lua_State* L, int parm, bool optional, double dfltval, bool* provided)
{
    if(provided) *provided = false;

    if(lua_isnumber(L, parm))
    {
        if(provided) *provided = true;
        return lua_tonumber(L, parm);
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw LuaException("must supply a floating point number for parameter #%d", parm);
    }
}

/*----------------------------------------------------------------------------
 * getLuaBoolean
 *----------------------------------------------------------------------------*/
bool LuaObject::getLuaBoolean (lua_State* L, int parm, bool optional, bool dfltval, bool* provided)
{
    if(provided) *provided = false;

    if(lua_isboolean(L, parm))
    {
        if(provided) *provided = true;
        return lua_toboolean(L, parm);
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw LuaException("must supply a boolean for parameter #%d", parm);
    }
}

/*----------------------------------------------------------------------------
 * getLuaString
 *----------------------------------------------------------------------------*/
const char* LuaObject::getLuaString (lua_State* L, int parm, bool optional, const char* dfltval, bool* provided)
{
    if(provided) *provided = false;

    if(lua_isstring(L, parm))
    {
        if(provided) *provided = true;
        return lua_tostring(L, parm);
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw LuaException("must supply a string for parameter #%d", parm);
    }
}

/*----------------------------------------------------------------------------
 * returnLuaStatus
 *----------------------------------------------------------------------------*/
int LuaObject::returnLuaStatus (lua_State* L, bool status, int num_obj_to_return)
{
    if(!status) lua_pushnil(L);
    else        lua_pushboolean(L, true);

    return num_obj_to_return;
}

/*----------------------------------------------------------------------------
 * releaseLuaObject
 *----------------------------------------------------------------------------*/
bool LuaObject::releaseLuaObject (void)
{
    bool is_delete_pending = false;

    /* Decrement Lock Count */
    lockCount--;

    /* Only on Last Release */
    if(lockCount == 0)
    {
        /* Get Lua Engine Object */
        lua_pushstring(LuaState, LuaEngine::LUA_SELFKEY);
        lua_gettable(LuaState, LUA_REGISTRYINDEX); /* retrieve value */
        LuaEngine* li = (LuaEngine*)lua_touserdata(LuaState, -1);
        lua_pop(LuaState, 1);
        if(li)
        {
            /* Release Object */
            li->releaseObject(lockKey);
            mlog(INFO, "Unlocking object %s of type %s, key = %ld\n", getName(), getType(), (unsigned long)lockKey);
        }
        else
        {
            mlog(CRITICAL, "Unable to retrieve lua engine needed to release object\n");
        }

        /* If an object has already been garabage collected, but the deletion was
        * delayed due to the object being locked, then when the lock is released
        * it needs to be deleted immediately. */
        is_delete_pending = pendingDelete;
    }
    else if(lockCount < 0)
    {
        mlog(CRITICAL, "Unmatched object release %s of type %s detected\n", getName(), getType());
    }

    return is_delete_pending;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaObject::LuaObject (lua_State* L, const char* object_type, const char* meta_name, const struct luaL_Reg meta_table[]):
    ObjectType(object_type),
    ObjectName(NULL),
    LuaMetaName(meta_name),
    LuaState(L)
{
    uint32_t engine_trace_id = ORIGIN;

    lockCount = 0;
    pendingDelete = false;
    objComplete = false;

    if(LuaState)
    {
        /* Trace from Lua Engine */
        lua_getglobal(LuaState, LuaEngine::LUA_TRACEID);
        engine_trace_id = lua_tonumber(LuaState, -1);

        /* Associate Meta Table */
        associateMetaTable(LuaState, meta_name, meta_table);
        mlog(INFO, "Created object of type %s/%s\n", getType(), LuaMetaName);
    }

    /* Start Trace */
    traceId = start_trace_ext(engine_trace_id, "lua_object", "{\"object_type\":\"%s\", \"meta_name\":\"%s\"}", object_type, meta_name);

}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaObject::~LuaObject (void)
{
    stop_trace(traceId);
    mlog(INFO, "Deleting %s of type %s\n", getName(), getType());
    if(ObjectName) delete [] ObjectName;
}

/*----------------------------------------------------------------------------
 * luaDelete
 *----------------------------------------------------------------------------*/
int LuaObject::luaDelete (lua_State* L)
{
    try
    {
        luaUserData_t* user_data = (luaUserData_t*)lua_touserdata(L, 1);
        if(user_data)
        {
            if(!user_data->alias)
            {
                LuaObject* lua_obj = user_data->luaObj;
                if(lua_obj)
                {
                    mlog(INFO, "Garbage collecting object %s of type %s\n", lua_obj->getName(), lua_obj->getType());
                    if(lua_obj->lockCount <= 0)
                    {
                        /* Delete Object */
                        delete lua_obj;
                        user_data->luaObj = NULL;
                    }
                    else
                    {
                        mlog(INFO, "Delaying delete on locked object %s\n", lua_obj->getName());
                        lua_obj->pendingDelete = true;
                    }
                }
                else
                {
                    /* This will occurr, for instance, when a device is closed
                    * explicitly, and then also deleted when the lua variable
                    * goes out of scope and is garbage collected */
                    mlog(INFO, "Vacuous delete of lua object that has already been deleted\n");
                }
            }
        }
        else
        {
            throw LuaException("unable to retrieve user data");
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error deleting object: %s\n", e.errmsg);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * luaAssociate
 *----------------------------------------------------------------------------*/
int LuaObject::luaAssociate(lua_State* L)
{
    try
    {
        /* Get Self */
        LuaObject* lua_obj = getLuaSelf(L, 1);

        /* Get Name */
        const char* name = getLuaString(L, 2, true, NULL);

        /* Associate Name */
        if(name)
        {
            if(lua_obj->ObjectName) delete [] lua_obj->ObjectName;
            lua_obj->ObjectName = StringLib::duplicate(name);
        }

        /* Return Name */
        lua_pushstring(L, lua_obj->ObjectName);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error associating object: %s\n", e.errmsg);
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaLock
 *----------------------------------------------------------------------------*/
int LuaObject::luaLock(lua_State* L)
{
    try
    {
        /* Get Self */
        LuaObject* lua_obj = getLuaSelf(L, 1);

        /* Lock Self */
        getLuaObject(L, 1, lua_obj->ObjectType);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error locking object: %s\n", e.errmsg);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * luaWaitOn - :waiton([<timeout is milliseconds>])
 *----------------------------------------------------------------------------*/
int LuaObject::luaWaitOn(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LuaObject* lua_obj = getLuaSelf(L, 1);

        /* Get Parameters */
        int timeout = getLuaInteger(L, 2, true, IO_PEND);

        /* Wait On Signal */
        lua_obj->objSignal.lock();
        {
            if(!lua_obj->objComplete)
            {
                lua_obj->objSignal.wait(SIGNAL_COMPLETE, timeout);
            }
            status = lua_obj->objComplete;
        }
        lua_obj->objSignal.unlock();
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error locking object: %s\n", e.errmsg);
    }

    /* Return Completion Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * associateMetaTable
 *----------------------------------------------------------------------------*/
void LuaObject::signalComplete (void)
{
    objSignal.lock();
    {
        if(!objComplete)
        {
            objSignal.signal(SIGNAL_COMPLETE);
        }
        objComplete = true;
    }
    objSignal.unlock();
}

/*----------------------------------------------------------------------------
 * associateMetaTable
 *----------------------------------------------------------------------------*/
void LuaObject::associateMetaTable (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[])
{
    if(luaL_newmetatable(L, meta_name))
    {
        /* Add Child Class Functions */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, meta_table, 0);

        /* Add Base Class Functions */
        LuaEngine::setAttrFunc(L, "name", luaAssociate);
        LuaEngine::setAttrFunc(L, "lock", luaLock);
        LuaEngine::setAttrFunc(L, "waiton", luaWaitOn);
        LuaEngine::setAttrFunc(L, "destroy", luaDelete);
        LuaEngine::setAttrFunc(L, "__gc", luaDelete);
    }
}

/*----------------------------------------------------------------------------
 * createLuaObject
 *
 *  Note: if object is an alias, all calls into it from Lua must be thread safe
 *----------------------------------------------------------------------------*/
int LuaObject::createLuaObject (lua_State* L, LuaObject* lua_obj, bool alias)
{
    /* Create Lua User Data Object */
    luaUserData_t* user_data = (luaUserData_t*)lua_newuserdata(L, sizeof(luaUserData_t));
    if(!user_data)
    {
        throw LuaException("failed to allocate new user data");
    }

    /* Return User Data to Lua */
    user_data->luaObj = lua_obj;
    user_data->alias = alias;
    luaL_getmetatable(L, lua_obj->LuaMetaName);
    lua_setmetatable(L, -2);
    return 1;
}

/*----------------------------------------------------------------------------
 * getLuaObject
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::getLuaObject (lua_State* L, int parm, const char* object_type, bool optional, LuaObject* dfltval)
{
    LuaObject* lua_obj = NULL;

    /* Get Lua Engine Object */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = (LuaEngine*)lua_touserdata(L, -1);
    if(!li) throw LuaException("Unable to retrieve lua engine");
    lua_pop(L, 1);

    /* Get User Data LuaObject */
    luaUserData_t* user_data = (luaUserData_t*)lua_touserdata(L, parm);
    if(user_data)
    {
        if(StringLib::match(object_type, user_data->luaObj->ObjectType))
        {
            lua_obj = user_data->luaObj;
            user_data->luaObj->lockCount++;

            /* Lock Object - Only for First Lock */
            if(user_data->luaObj->lockCount == 1)
            {
                lua_obj->lockKey = li->lockObject(lua_obj);
                mlog(INFO, "Locking object %s of type %s, key = %ld\n", lua_obj->getName(), lua_obj->getType(), (unsigned long)lua_obj->lockKey);
            }
        }
        else
        {
            throw LuaException("%s object returned incorrect type <%s.%s>", object_type, user_data->luaObj->ObjectType, user_data->luaObj->LuaMetaName);
        }
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw LuaException("calling object method from something not an object");
    }

    return lua_obj;
}

/*----------------------------------------------------------------------------
 * getLuaSelf
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::getLuaSelf (lua_State* L, int parm)
{
    luaUserData_t* user_data = (luaUserData_t*)lua_touserdata(L, parm);
    if(user_data)
    {
        if(user_data->luaObj)
        {
            if(luaL_testudata(L, parm, user_data->luaObj->LuaMetaName))
            {
                return user_data->luaObj;
            }
            else
            {
                throw LuaException("object method called from inconsistent type <%s>", user_data->luaObj->LuaMetaName);
            }
        }
        else
        {
            throw LuaException("object method called on emtpy object");
        }
    }
    else
    {
        throw LuaException("calling object method from something not an object");
    }
}
