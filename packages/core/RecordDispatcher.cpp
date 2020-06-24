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

#include "RecordDispatcher.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Dictionary<RecordDispatcher::calcFunc_t> RecordDispatcher::keyCalcFunctions;

const char* RecordDispatcher::LuaMetaName = "RecordDispatcher";
const struct luaL_Reg RecordDispatcher::LuaMetaTable[] = {
    {"attach",      luaAttachDispatch},
    {"clear",       luaClearError},
    {"drain",       luaDrain},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - dispatcher(<input stream name>, [<num threads>], [<key mode>, <key parm>])
 *----------------------------------------------------------------------------*/
int RecordDispatcher::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* qname           = getLuaString(L, 1);
        long        num_threads     = getLuaInteger(L, 2, true, LocalLib::nproc());
        const char* key_mode_str    = getLuaString(L, 3, true, "RECEIPT_KEY");

        /* Check Number of Threads */
        if(num_threads < 1)
        {
            throw LuaException("invalid number of threads supplied (must be >= 1)");
        }

        /* Set Key Mode */
        keyMode_t key_mode = str2mode(key_mode_str);
        const char* key_field = NULL;
        calcFunc_t  key_func = NULL;
        if(key_mode == INVALID_KEY_MODE)
        {
            throw LuaException("Invalid key mode specified: %s\n", key_mode_str);
        }
        else if(key_mode == FIELD_KEY_MODE)
        {
            key_field = getLuaString(L, 4);
        }
        else if(key_mode == CALCULATED_KEY_MODE)
        {
            const char* key_func_str = getLuaString(L, 4);
            key_func = keyCalcFunctions[key_func_str];
        }

        /* Create Record Dispatcher */
        return createLuaObject(L, new RecordDispatcher(L, qname, key_mode, key_field, key_func, num_threads));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
    catch(std::out_of_range& e)
    {
        (void)e;
        mlog(CRITICAL, "Invalid calculation function provided - no handler installed\n");
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * str2mode
 *----------------------------------------------------------------------------*/
RecordDispatcher::keyMode_t RecordDispatcher::str2mode(const char* str)
{
         if(StringLib::match(str, "FIELD_KEY"))         return FIELD_KEY_MODE;
    else if(StringLib::match(str, "RECEIPT_KEY"))       return RECEIPT_KEY_MODE;
    else if(StringLib::match(str, "CALCULATED_KEY"))    return CALCULATED_KEY_MODE;
    else                                                return INVALID_KEY_MODE;
}

/*----------------------------------------------------------------------------
 * mode2str
 *----------------------------------------------------------------------------*/
const char* RecordDispatcher::mode2str(keyMode_t mode)
{
    switch(mode)
    {
        case FIELD_KEY_MODE:        return "FIELD_KEY";
        case RECEIPT_KEY_MODE:      return "RECEIPT_KEY";
        case CALCULATED_KEY_MODE:   return "CALCULATED_KEY";
        default:                    return "INVALID_KEY";
    }
}

/*----------------------------------------------------------------------------
 * addKeyCalcFunc
 *----------------------------------------------------------------------------*/
bool RecordDispatcher::addKeyCalcFunc(const char* calc_name, calcFunc_t func)
{
    return keyCalcFunctions.add(calc_name, func);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RecordDispatcher::RecordDispatcher(lua_State* L, const char* inputq_name, keyMode_t key_mode, const char* key_field, calcFunc_t key_func, int num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(inputq_name);
    assert(num_threads > 0);

    /* Initialize Attributes */
    keyMode         = key_mode;
    keyRecCnt       = 0;
    keyField        = StringLib::duplicate(key_field);
    keyFunc         = key_func;
    numThreads      = num_threads;
    recError        = false;

    /* Create Subscriber */
    inQ = new Subscriber(inputq_name);

    /* Create Thread Pool */
    threadPool = new Thread* [numThreads];
    for(int i = 0; i < numThreads; i++) threadPool[i] = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RecordDispatcher::~RecordDispatcher(void)
{
    stopThreads();
    delete [] threadPool;
    delete inQ;
    if (keyField) delete [] keyField;

    dispatch_t dispatch;
    const char* key = dispatchTable.first(&dispatch);
    while(key != NULL)
    {
        for(int d = 0; d < dispatch.size; d++)
        {
            dispatch.list[d]->removeLock();
        }
        delete [] dispatch.list;
        key = dispatchTable.next(&dispatch);
    }
}

/*----------------------------------------------------------------------------
 * createRecord
 *----------------------------------------------------------------------------*/
RecordObject* RecordDispatcher::createRecord (unsigned char* buffer, int size)
{
    return new RecordInterface(buffer, size);
}

/*----------------------------------------------------------------------------
 * luaAttachDispatch - :attach(<dispatch object>, [<record type name 1>, ..., <record type name N]>)
 *----------------------------------------------------------------------------*/
int RecordDispatcher::luaAttachDispatch(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        RecordDispatcher* lua_obj = (RecordDispatcher*)getLuaSelf(L, 1);

        /* Get Parameters */
        DispatchObject* dispatch    = (DispatchObject*)lockLuaObject(L, 2, DispatchObject::OBJECT_TYPE);
        int             num_parms   = getLuaNumParms(L);

        /* Stop Worker Threads */
        lua_obj->stopThreads();

        /* Attach Dispatches */
        for(int p = 3; p <= num_parms; p++)
        {
            List<DispatchObject*> new_dispatch_list;

            /* Build Record Type */
            char arch_rec_type[MAX_STR_SIZE]; // used as buffer if necessary
            const char* rec_type_str = getLuaString(L, p);
            const char* rec_type = RecordObject::buildRecType(rec_type_str, arch_rec_type, MAX_STR_SIZE);

            try
            {
                dispatch_t& old_dispatch = lua_obj->dispatchTable[rec_type];
                if(old_dispatch.list)
                {
                    /* Check List Doesn't Already Contain Dispatch */
                    for(int d = 0; d < old_dispatch.size; d++)
                    {
                        if(old_dispatch.list[d] == dispatch)
                        {
                            throw LuaException("dispatch already attached to %s", rec_type_str);
                        }
                    }

                    /* Copy Dispatches Over to New List */
                    for(int d = 0; d < old_dispatch.size; d++)
                    {
                        new_dispatch_list.add(old_dispatch.list[d]);
                    }

                    /* Remove Old Dispatch List */
                    delete [] old_dispatch.list;
                }
            }
            catch(std::out_of_range& e)
            {
                (void)e;
            }

            /* Attach Dispatch */
            new_dispatch_list.add(dispatch);

            /* Create New Dispatch Table Entry */
            dispatch_t new_dispatch = { NULL, new_dispatch_list.length() };
            new_dispatch.list = new DispatchObject* [new_dispatch.size];
            for(int d = 0; d < new_dispatch.size; d++)
            {
                new_dispatch.list[d] = new_dispatch_list[d];
            }

            /* Replace Dispatch Table Entry */
            lua_obj->dispatchTable.add(rec_type, new_dispatch);
        }

        /* Add Dispatch to List */
        lua_obj->dispatchList.add(dispatch);

        /* Start Worker Threads */
        lua_obj->startThreads();

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error attaching dispatch: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaClearError - :clearerror()
 *----------------------------------------------------------------------------*/
int RecordDispatcher::luaClearError(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        RecordDispatcher* lua_obj = (RecordDispatcher*)getLuaSelf(L, 1);

        /* Clear Errors */
        lua_obj->recError = false;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error clearing errors: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaDrain - :drain()
 *----------------------------------------------------------------------------*/
int RecordDispatcher::luaDrain (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        RecordDispatcher* lua_obj = (RecordDispatcher*)getLuaSelf(L, 1);

        /* Clear Errors */
        lua_obj->inQ->drain();

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error draining input stream: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * startThreads
 *----------------------------------------------------------------------------*/
void RecordDispatcher::startThreads (void)
{
    dispatcherActive = true;
    for(int i = 0; i < numThreads; i++) threadPool[i] = new Thread(dispatcherThread, this);
}

/*----------------------------------------------------------------------------
 * stopThreads
 *----------------------------------------------------------------------------*/
void RecordDispatcher::stopThreads(void)
{
    dispatcherActive = false;
    for(int i = 0; i < numThreads; i++) if (threadPool[i]) delete threadPool[i];
}

/*----------------------------------------------------------------------------
 * dispatcherThread
 *----------------------------------------------------------------------------*/
void* RecordDispatcher::dispatcherThread(void* parm)
{
    RecordDispatcher* dispatcher = (RecordDispatcher*)parm;

    bool self_delete = false;

    /* Loop Forever */
    while(dispatcher->dispatcherActive)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        int recv_status = dispatcher->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            unsigned char* msg = (unsigned char*)ref.data;
            int len = ref.size;

            /* Dispatch Record */
            try
            {
                RecordObject* record = dispatcher->createRecord(msg, len);
                dispatcher->dispatchRecord(record);
                delete record;
            }
            catch (const InvalidRecordException& e)
            {
                if(!dispatcher->recError)
                {
                    int num_newlines = len / 16 + 3;
                    char* msg_str = new char[len * 2 + num_newlines + 1];
                    mlog(CRITICAL, "%s unable to create record from message: %s\n", dispatcher->ObjectType, e.what());
                    int msg_index = 0;
                    for(int i = 0; i < len; i++)
                    {
                        sprintf(&msg_str[msg_index], "%02X", msg[i]);
                        msg_index += 2;
                        if(i % 16 == 15) msg_str[msg_index++] = '\n';
                    }
                    msg_str[msg_index++] = '\n';
                    msg_str[msg_index++] = '\0';
                    mlog(INFO, "%s", msg_str);
                    delete [] msg_str;
                }
                dispatcher->recError = true;
            }

            /* Dereference Message */
            dispatcher->inQ->dereference(ref);
        }
        else if(recv_status == MsgQ::STATE_TIMEOUT)
        {
            /* Signal Timeout to Dispatches */
            for(int d = 0; dispatcher->dispatchList.length(); d++)
            {
                DispatchObject* dis = dispatcher->dispatchList[d];
                dis->processTimeout();
            }
        }
        else
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive in %s with error %d\n", dispatcher->ObjectType, recv_status);
            dispatcher->dispatcherActive = false; // breaks out of loop
            self_delete = true;
        }
    }

    /* Check Status */
    if (self_delete)
    {
        mlog(CRITICAL, "Fatal error detected in %s, exiting processor\n", dispatcher->ObjectType);
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * dispatchRecord
 *----------------------------------------------------------------------------*/
void RecordDispatcher::dispatchRecord (RecordObject* record)
{
    /* Dispatch Dispatches */
    try
    {
        dispatch_t& dis = dispatchTable[record->getRecordType()];

        /* Get Key */
        okey_t key = 0;
        if(keyMode == FIELD_KEY_MODE)
        {
            RecordObject::field_t key_field = record->getField(keyField);
            key = (okey_t)record->getValueInteger(key_field);
        }
        else if(keyMode == RECEIPT_KEY_MODE)
        {
            dispatchMutex.lock();
            {
                key = keyRecCnt++;
            }
            dispatchMutex.unlock();
        }
        else if(keyMode == CALCULATED_KEY_MODE)
        {
            key = keyFunc(record->getRecordData(), record->getRecordDataSize());
        }

        /* Process Record */
        for (int i = 0; i < dis.size; i++)
        {
            dis.list[i]->processRecord(record, key);
        }
    }
    catch(std::out_of_range& e)
    {
        (void)e;
    }
}
