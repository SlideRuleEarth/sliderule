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

#include "LuaObject.h"
#include "LuaEngine.h"
#include "EventLib.h"
#include "StringLib.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaObject::BASE_OBJECT_TYPE = "LuaObject";
Dictionary<LuaObject::global_object_t> LuaObject::globalObjects;
Mutex LuaObject::globalMut;
std::atomic<long> LuaObject::numObjects(0);

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaObject::~LuaObject (void)
{
    stop_trace(DEBUG, traceId);
    mlog(DEBUG, "Deleting %s/%s", getType(), getName());

    /* Remove Name from Global Objects */
    globalMut.lock();
    {
        /* Remove Previous Name (if it exists) */
        if(ObjectName)
        {
            globalObjects.remove(ObjectName);
            delete [] ObjectName;
        }
    }
    globalMut.unlock();

    /* Count Object */
    numObjects--;
}

/*----------------------------------------------------------------------------
 * getType
 *----------------------------------------------------------------------------*/
const char* LuaObject::getType (void) const
{
    if(ObjectType) return ObjectType;
    return "<untyped>";
}

/*----------------------------------------------------------------------------
 * getName
 *----------------------------------------------------------------------------*/
const char* LuaObject::getName (void) const
{
    if(ObjectName) return ObjectName;
    return "<unnamed>";
}

/*----------------------------------------------------------------------------
 * getLuaNumParms
 *----------------------------------------------------------------------------*/
int LuaObject::getLuaNumParms (lua_State* L)
{
    return lua_gettop(L);
}

/*----------------------------------------------------------------------------
 * luaGetByName
 *----------------------------------------------------------------------------*/
int LuaObject::luaGetByName(lua_State* L)
{
    try
    {
        LuaObject* lua_obj = NULL;
        globalMut.lock();
        {
            /* Get Parameters */
            const char* name = getLuaString(L, 1);

            /* Get Self */
            lua_obj = globalObjects.get(name).lua_obj;

            /* Associate MetaTable with LuaObject */
            associateMetaTable(L, lua_obj->LuaMetaName, lua_obj->LuaMetaTable);
        }
        globalMut.unlock();

        /* Return Lua Object */
        return createLuaObject(L, lua_obj);
    }
    catch(const RunTimeException& e)
    {
        globalMut.unlock();
        mlog(DEBUG, "Failed to get Lua object by name: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
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

    if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "must supply an integer for parameter #%d", parm);
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

    if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "must supply a floating point number for parameter #%d", parm);
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

    if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "must supply a boolean for parameter #%d", parm);
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

    if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "must supply a string for parameter #%d", parm);
}

/*----------------------------------------------------------------------------
 * getLuaObject
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::getLuaObject (lua_State* L, int parm, const char* object_type, bool optional, LuaObject* dfltval)
{
    LuaObject* lua_obj = NULL;
    luaUserData_t* user_data = static_cast<luaUserData_t*>(lua_touserdata(L, parm));
    if(user_data)
    {
        if(StringLib::match(object_type, user_data->luaObj->ObjectType))
        {
            lua_obj = user_data->luaObj;
            user_data->luaObj->referenceCount++;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s object returned incorrect type <%s.%s>", object_type, user_data->luaObj->ObjectType, user_data->luaObj->LuaMetaName);
        }
    }
    else if(optional && ((lua_gettop(L) < parm) || lua_isnil(L, parm)))
    {
        return dfltval;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "calling object method from something not an object");
    }

    return lua_obj;
}

/*----------------------------------------------------------------------------
 * returnLuaStatus
 *----------------------------------------------------------------------------*/
int LuaObject::returnLuaStatus (lua_State* L, bool status, int num_obj_to_return)
{
    if(!status) lua_pushnil(L);
    else
    {
        if( num_obj_to_return == 1 )
        {
            const int stack_cnt = lua_gettop(L);

            /* Self object must be on stack */
            assert(stack_cnt != 0);

            lua_pop(L, stack_cnt - 1);
            /* Return self as status, allow to chain calls */
        }
        else lua_pushboolean(L, true);
    }

    return num_obj_to_return;
}

/*----------------------------------------------------------------------------
 * freeGlobalObjects
 *----------------------------------------------------------------------------*/
void LuaObject::freeGlobalObjects (void)
{
    globalMut.lock();
    {
        char** keys = NULL;
        const int num_keys = globalObjects.getKeys(&keys);
        for(int i = 0; i < num_keys; i++)
        {
            LuaObject* lua_obj = globalObjects.get(keys[i]).lua_obj;  // NOLINT(clang-analyzer-core.CallAndMessage)
            lua_obj->releaseLuaObject();
            delete [] keys[i];
        }
        delete [] keys;
    }
    globalMut.unlock();
}

/*----------------------------------------------------------------------------
 * getGlobalObjects
 *----------------------------------------------------------------------------*/
void LuaObject::getGlobalObjects (vector<object_info_t>& globals)
{
    globalMut.lock();
    {
        global_object_t global_object;
        const char* object_name = globalObjects.first(&global_object);
        while(object_name != NULL)
        {
            const object_info_t info = {
                .objName = object_name,
                .objType = global_object.lua_obj->getType(),
                .refCnt = global_object.lua_obj->referenceCount
            };
            globals.push_back(info);
            object_name = globalObjects.next(&global_object);
        }
    }
    globalMut.unlock();
}

/*----------------------------------------------------------------------------
 * getNumObjects
 *----------------------------------------------------------------------------*/
long LuaObject::getNumObjects (void)
{
    const long num_objects = numObjects;
    return num_objects;
}

/*----------------------------------------------------------------------------
 * createLuaObject
 *
 *  Note: if object is an alias, all calls into it from Lua must be thread safe
 *----------------------------------------------------------------------------*/
int LuaObject::createLuaObject (lua_State* L, LuaObject* lua_obj)
{
    /* Create Lua User Data Object */
    lua_obj->userData = static_cast<luaUserData_t*>(lua_newuserdata(L, sizeof(luaUserData_t)));
    if(!lua_obj->userData)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to allocate new user data");
    }

    /* Bump Reference Count */
    lua_obj->referenceCount++;

    /* Return User Data to Lua */
    lua_obj->userData->luaObj = lua_obj;
    luaL_getmetatable(L, lua_obj->LuaMetaName);
    lua_setmetatable(L, -2);
    return 1;
}

/*----------------------------------------------------------------------------
 * getLuaObjectByName
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::getLuaObjectByName (const char* name, const char* object_type)
{
    LuaObject* lua_obj = NULL;
    globalMut.lock();
    {
        try
        {
            LuaObject* obj = globalObjects.get(name).lua_obj;
            if(StringLib::match(obj->getType(), object_type))
            {
                lua_obj = obj;
                lua_obj->referenceCount++;
            }
        }
        catch(const RunTimeException& e)
        {
        }
    }
    globalMut.unlock();

    return lua_obj;
}

/*----------------------------------------------------------------------------
 * referenceLuaObject
 *----------------------------------------------------------------------------*/
void LuaObject::referenceLuaObject (LuaObject* lua_obj)
{
    globalMut.lock();
    {
        lua_obj->referenceCount++;
    }
    globalMut.unlock();
}

/*----------------------------------------------------------------------------
 * releaseLuaObject
 *----------------------------------------------------------------------------*/
bool LuaObject::releaseLuaObject (void)
{
    bool is_delete_pending = false;

    /* Decrement Reference Count */
    referenceCount--;
    if(referenceCount == 0)
    {
        mlog(DEBUG, "Delete on release for object %s/%s", getType(), getName());
        is_delete_pending = true;
    }
    else if(referenceCount < 0)
    {
        mlog(CRITICAL, "Unmatched object release %s of type %s detected", getName(), getType());
    }

    /* Delete THIS Object */
    if(is_delete_pending)
    {
        if(userData) userData->luaObj = NULL;
        delete this;
    }

    return is_delete_pending;
}

/*----------------------------------------------------------------------------
 * waitComplete
 *----------------------------------------------------------------------------*/
bool LuaObject::waitComplete (int timeout)
{
    bool status = false;
    objSignal.lock();
    {
        if(!objComplete)
        {
            objSignal.wait(SIGNAL_COMPLETE, timeout);
        }
        status = objComplete;
    }
    objSignal.unlock();
    return status;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaObject::LuaObject (lua_State* L, const char* object_type, const char* meta_name, const struct luaL_Reg meta_table[]):
    ObjectType(object_type),
    ObjectName(NULL),
    LuaMetaName(meta_name),
    LuaMetaTable(meta_table),
    LuaState(L),
    referenceCount(0),
    userData(NULL),
    objComplete(false)
{
    uint32_t engine_trace_id = ORIGIN;

    if(LuaState)
    {
        /* Trace from Lua Engine */
        lua_getglobal(LuaState, LuaEngine::LUA_TRACEID);
        engine_trace_id = lua_tonumber(LuaState, -1);

        /* Associate Meta Table */
        associateMetaTable(LuaState, meta_name, meta_table);
        mlog(DEBUG, "Created object of type %s/%s", getType(), LuaMetaName);
    }

    /* Count Object */
    numObjects++;

    /* Start Trace */
    traceId = start_trace(DEBUG, engine_trace_id, "lua_object", "{\"object_type\":\"%s\", \"meta_name\":\"%s\"}", object_type, meta_name);
}

/*----------------------------------------------------------------------------
 * signalComplete
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
        LuaEngine::setAttrFunc(L, "name", luaName);
        LuaEngine::setAttrFunc(L, "getbyname", luaGetByName);
        LuaEngine::setAttrFunc(L, "waiton", luaWaitOn);
        LuaEngine::setAttrFunc(L, "destroy", luaDestroy);
        LuaEngine::setAttrFunc(L, "__gc", luaDelete);
    }
}

/*----------------------------------------------------------------------------
 * getLuaSelf
 *----------------------------------------------------------------------------*/
LuaObject* LuaObject::getLuaSelf (lua_State* L, int parm)
{
    luaUserData_t* user_data = static_cast<luaUserData_t*>(lua_touserdata(L, parm));
    if(user_data)
    {
        if(user_data->luaObj)
        {
            if(luaL_testudata(L, parm, user_data->luaObj->LuaMetaName))
            {
                return user_data->luaObj;
            }

            throw RunTimeException(CRITICAL, RTE_FAILURE, "object method called from inconsistent type <%s>", user_data->luaObj->LuaMetaName);
        }

        throw RunTimeException(CRITICAL, RTE_FAILURE, "object method called on emtpy object");
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "calling object method from something not an object");
}

/*----------------------------------------------------------------------------
 * luaDelete - called only by the garbage collector
 *----------------------------------------------------------------------------*/
int LuaObject::luaDelete (lua_State* L)
{
    try
    {
        luaUserData_t* user_data = static_cast<luaUserData_t*>(lua_touserdata(L, 1));
        if(user_data)
        {
            LuaObject* lua_obj = user_data->luaObj;
            if(lua_obj)
            {
                const int count = lua_obj->referenceCount--;
                mlog(DEBUG, "Garbage collecting object %s/%s <%d>", lua_obj->getType(), lua_obj->getName(), count);

                if(lua_obj->referenceCount == 0)
                {
                    /* Delete Object */
                    delete lua_obj;
                    user_data->luaObj = NULL;
                }
                else
                {
                    mlog(DEBUG, "Delaying delete on referenced object %s/%s <%d>", lua_obj->getType(), lua_obj->getName(), count);
                    lua_obj->userData = NULL; // user data is now out of scope
                }
            }
            else
            {
                /* This will occurr, for instance, when a device is closed
                 * explicitly, and then also deleted when the lua variable
                 * goes out of scope and is garbage collected */
                mlog(DEBUG, "Vacuous delete of lua object that has already been deleted");
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to retrieve user data");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error deleting object: %s", e.what());
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * luaDestroy - called explicitly by scripts
 *----------------------------------------------------------------------------*/
int LuaObject::luaDestroy (lua_State* L)
{
    try
    {
        luaUserData_t* user_data = static_cast<luaUserData_t*>(lua_touserdata(L, 1));
        if(user_data)
        {
            LuaObject* lua_obj = user_data->luaObj;
            if(lua_obj)
            {
                const int count = lua_obj->referenceCount--;
                mlog(DEBUG, "Destroying object %s/%s <%d>", lua_obj->getType(), lua_obj->getName(), count);

                if(lua_obj->referenceCount == 0)
                {
                    /* Delete Object */
                    delete lua_obj;
                    user_data->luaObj = NULL;
                }
                else
                {
                    mlog(DEBUG, "Delaying destroy on referenced object %s/%s <%d>", lua_obj->getType(), lua_obj->getName(), count);
                }
            }
            else
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "Attempting to destroy lua object that has already been deleted");
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to retrieve user data");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error destroying object: %s", e.what());
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * luaName
 *----------------------------------------------------------------------------*/
int LuaObject::luaName(lua_State* L)
{
    try
    {
         bool status = false;

       /* Get Self */
        LuaObject* lua_obj = getLuaSelf(L, 1);

        /* Get Name */
        const char* name = getLuaString(L, 2);

        /* Add Name to Global Objects */
        globalMut.lock();
        {
            /* Check if Name Already Exists */
            if(!lua_obj->ObjectName)
            {
                /* Register Name */
                const global_object_t global_object = { .lua_obj = lua_obj };
                if(globalObjects.add(name, global_object, true))
                {
                    /* Bump Reference Count
                     *  this make the object global as there will now be an additional reference
                     *  to this object without a corresponding lua variable that will get
                     *  garbage collected */
                    lua_obj->referenceCount++;

                    /* Associate Name */
                    lua_obj->ObjectName = StringLib::duplicate(name);
                    mlog(DEBUG, "Associating %s with object of type %s", lua_obj->getName(), lua_obj->getType());
                    status = true;
                }
                else
                {
                    mlog(WARNING, "Name conflict on %s for type %s", lua_obj->getName(), lua_obj->getType());
                    status = true; // do not treat as error
                }
            }
            else
            {
                mlog(WARNING, "Object already named %s, cannot overwrite with name %s", lua_obj->getName(), name);
                status = true; // do not treat as error
            }
        }
        globalMut.unlock();

        /* Check for Errors */
        if(!status) throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to register name: %s", name);

        /* Pop name */
        lua_pop(L, 1);

        /* Stack holds Self */
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error associating object: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
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
        const int timeout = getLuaInteger(L, 2, true, IO_PEND);
        const char* rspq = getLuaString(L, 3, true, NULL);
        int interval = getLuaInteger(L, 4, true, DEFAULT_WAIT_INTERVAL);

        /* Wait On Signal */
        if(rspq && timeout > 0)
        {
            Publisher pub(rspq);
            int duration = 0;
            interval = MIN(interval, timeout);

            while(!status)
            {
                status = lua_obj->waitComplete(interval);
                if(!status)
                {
                    if(pub.getSubCnt() <= 0)
                    {
                        alert(ERROR, RTE_TIMEOUT, &pub, NULL, "request <%s> terminated while waiting", rspq);
                        break;
                    }
                    else if(duration >= timeout)
                    {
                        alert(ERROR, RTE_TIMEOUT, &pub, NULL, "request <%s> timed-out after %d seconds", rspq, timeout);
                        break;
                    }
                    else
                    {
                        duration += interval;
                        alert(INFO, RTE_TIMEOUT, &pub, NULL, "request <%s> ... %s still running after %d seconds", rspq, lua_obj->getName(), duration / 1000);
                    }
                }
            }
        }
        else
        {
            status = lua_obj->waitComplete(timeout);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error waiting on object: %s", e.what());
    }

    /* Return Completion Status */
    return returnLuaStatus(L, status);
}

