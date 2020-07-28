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
#include "Ordering.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaObject::BASE_OBJECT_TYPE = "LuaObject";
const char* LuaException::ERROR_TYPE = "LuaException";

Ordering<LuaObject*> LuaObject::lockList;
Mutex LuaObject::lockMut;
okey_t LuaObject::currentLockKey = 0;

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
 * releaseLuaObjects
 *----------------------------------------------------------------------------*/
void LuaObject::releaseLuaObjects (void)
{
    LuaObject* lua_obj;
    okey_t key = lockList.first(&lua_obj);
    while(key != Ordering<LuaObject*>::INVALID_KEY)
    {
        if(lua_obj)
        {
            delete lua_obj;
        }
        else
        {
            mlog(CRITICAL, "Double delete of object detected, key = %ld\n", (unsigned long)key);
        }

        key = lockList.next(&lua_obj);
    }
    lockList.clear();
}

/*----------------------------------------------------------------------------
 * removeLock
 *----------------------------------------------------------------------------*/
void LuaObject::removeLock (void)
{
    lockMut.lock();
    {
        if(isLocked)
        {
            mlog(INFO, "Unlocking object %s of type %s, key = %ld\n", getName(), getType(), (unsigned long)lockKey);
            isLocked = false;
            lockList.remove(lockKey);
        }
    }
    lockMut.unlock();
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

    isLocked = false;

    if(L)
    {
        /* Trace from Lua Engine */
        lua_getglobal(L, LuaEngine::LUA_TRACEID);
        engine_trace_id = lua_tonumber(L, -1);

        /* Associate Meta Table */
        associateMetaTable(L, meta_name, meta_table);
        mlog(INFO, "Created object of type %s\n", getType());
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
        LuaEngine::setAttrFunc(L, "name", associateLuaName);
        LuaEngine::setAttrFunc(L, "destroy", deleteLuaObject);
        LuaEngine::setAttrFunc(L, "__gc", deleteLuaObject);
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
 * deleteLuaObject
 *----------------------------------------------------------------------------*/
int LuaObject::deleteLuaObject (lua_State* L)
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
                    lockMut.lock();
                    {
                        if(!lua_obj->isLocked)
                        {
                            /* Delete Object */
                            delete lua_obj;
                            user_data->luaObj = NULL;
                        }
                        else
                        {
                            mlog(INFO, "Delaying delete on locked object %s\n", lua_obj->getName());
                        }
                    }
                    lockMut.unlock();
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
 * associateLuaName
 *----------------------------------------------------------------------------*/
int LuaObject::associateLuaName(lua_State* L)
{
    try
    {
        /* Get Self */
        LuaObject* lua_obj = getLuaSelf(L, 1);

        /* Get Name */
        const char* name = getLuaString(L, 2);

        /* Associate Name */
        if(lua_obj->ObjectName) delete [] lua_obj->ObjectName;
        lua_obj->ObjectName = StringLib::duplicate(name);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error associating object: %s\n", e.errmsg);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * lockLuaObject
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::lockLuaObject (lua_State* L, int parm, const char* object_type, bool optional, LuaObject* dfltval)
{
    LuaObject* lua_obj = NULL;

    luaUserData_t* user_data = (luaUserData_t*)lua_touserdata(L, parm);
    if(user_data)
    {
        if(StringLib::match(object_type, user_data->luaObj->ObjectType))
        {
            lockMut.lock();
            {
                if(!user_data->luaObj->isLocked)
                {
                    lua_obj = user_data->luaObj;
                    lua_obj->isLocked = true;
                    lua_obj->lockKey = currentLockKey++;
                    lockList.add(lua_obj->lockKey, lua_obj);
                    mlog(INFO, "Locking object %s of type %s, key = %ld\n", lua_obj->getName(), lua_obj->getType(), (unsigned long)lua_obj->lockKey);
                }
            }
            lockMut.unlock();

            if(lua_obj == NULL)
            {
                throw LuaException("%s object %s is locked", object_type, user_data->luaObj->getName());
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
