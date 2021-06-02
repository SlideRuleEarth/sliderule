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

#include "RecordDispatcher.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Dictionary<RecordDispatcher::calcFunc_t> RecordDispatcher::keyCalcFunctions;

const char* RecordDispatcher::LuaMetaName = "RecordDispatcher";
const struct luaL_Reg RecordDispatcher::LuaMetaTable[] = {
    {"run",         luaRun},
    {"attach",      luaAttachDispatch},
    {"clear",       luaClearError},
    {"drain",       luaDrain},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - dispatcher(<input stream name>, [<num threads>], [<key mode>, <key parm>], [<subscriber type>])
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
            throw RunTimeException(CRITICAL, "invalid number of threads supplied (must be >= 1)");
        }

        /* Set Key Mode */
        keyMode_t key_mode = str2mode(key_mode_str);
        const char* key_field = NULL;
        calcFunc_t  key_func = NULL;
        if(key_mode == INVALID_KEY_MODE)
        {
            throw RunTimeException(CRITICAL, "Invalid key mode specified: %s", key_mode_str);
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

        /* Set Subscriber Type */
        MsgQ::subscriber_type_t type = (MsgQ::subscriber_type_t)getLuaInteger(L, 5, true, MsgQ::SUBSCRIBER_OF_CONFIDENCE);

        /* Create Record Dispatcher */
        return createLuaObject(L, new RecordDispatcher(L, qname, key_mode, key_field, key_func, num_threads, type));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
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
RecordDispatcher::RecordDispatcher( lua_State* L, const char* inputq_name, 
                                    keyMode_t key_mode, const char* key_field, calcFunc_t key_func, 
                                    int num_threads, MsgQ::subscriber_type_t type):
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
    threadsComplete = 0;
    recError        = false;

    /* Create Subscriber */
    inQ = new Subscriber(inputq_name, type);

    /* Create Thread Pool */
    dispatcherActive = false;
    threadPool = new Thread* [numThreads];
    for(int i = 0; i < numThreads; i++) threadPool[i] = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RecordDispatcher::~RecordDispatcher(void)
{
    dispatcherActive = false;
    for(int i = 0; i < numThreads; i++)
    {
        if (threadPool[i]) delete threadPool[i];
    }
    delete [] threadPool;

    delete inQ;

    if (keyField) delete [] keyField;

    dispatch_t dispatch;
    const char* key = dispatchTable.first(&dispatch);
    while(key != NULL)
    {
        for(int d = 0; d < dispatch.size; d++)
        {
            dispatch.list[d]->releaseLuaObject();
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
 * luaRun - :run()
 *----------------------------------------------------------------------------*/
int RecordDispatcher::luaRun(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        RecordDispatcher* lua_obj = (RecordDispatcher*)getLuaSelf(L, 1);

        /* Start Threads */
        lua_obj->dispatcherActive = true;
        for(int i = 0; i < lua_obj->numThreads; i++)
        {
            lua_obj->threadPool[i] = new Thread(dispatcherThread, lua_obj);
        }

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error starting dispatcher: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
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
        int             num_parms   = getLuaNumParms(L);
        DispatchObject* dispatch    = (DispatchObject*)getLuaObject(L, 2, DispatchObject::OBJECT_TYPE);

        /* Check if Active */
        if(lua_obj->dispatcherActive)
        {
            throw RunTimeException(CRITICAL, "Cannot attach %s to a running dispatcher", dispatch->getName());
        }

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
                            throw RunTimeException(CRITICAL, "Dispatch already attached to %s", rec_type_str);
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
            catch(RunTimeException& e)
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

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error attaching dispatch: %s", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error clearing errors: %s", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error draining input stream: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * dispatcherThread
 *----------------------------------------------------------------------------*/
void* RecordDispatcher::dispatcherThread(void* parm)
{
    RecordDispatcher* dispatcher = (RecordDispatcher*)parm;

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
            if(len > 0)
            {
                try
                {
                    /* Create & Dispatch Record */
                    RecordObject* record = dispatcher->createRecord(msg, len);
                    dispatcher->dispatchRecord(record);
                    delete record;
                }
                catch (const RunTimeException& e)
                {
                    if(!dispatcher->recError)
                    {
                        int num_newlines = len / 16 + 3;
                        char* msg_str = new char[len * 2 + num_newlines + 1];
                        mlog(e.level(), "%s unable to create record from message: %s", dispatcher->ObjectType, e.what());
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
            }
            else
            {
                /* Terminating Message */
                mlog(INFO, "Terminator received on %s, exiting dispatcher", dispatcher->inQ->getName());
                dispatcher->dispatcherActive = false; // breaks out of loop
            }

            /* Dereference Message */
            dispatcher->inQ->dereference(ref);
        }
        else if(recv_status == MsgQ::STATE_TIMEOUT)
        {
            /* Signal Timeout to Dispatches */
            int num_dispatches = dispatcher->dispatchList.length();
            for(int d = 0; d < num_dispatches; d++)
            {
                DispatchObject* dis = dispatcher->dispatchList[d];
                dis->processTimeout();
            }
        }
        else
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d", dispatcher->inQ->getName(), recv_status);
            dispatcher->dispatcherActive = false; // breaks out of loop
        }
    }

    /* Handle Termination */
    dispatcher->threadMut.lock();
    {
        dispatcher->threadsComplete++;
        if(dispatcher->threadsComplete == dispatcher->numThreads)
        {
            /* Process Termination for each Dispatch */
            dispatch_t dispatch;
            const char* key = dispatcher->dispatchTable.first(&dispatch);
            while(key != NULL)
            {
                for(int d = 0; d < dispatch.size; d++)
                {
                    if(!dispatch.list[d]->processTermination())
                    {
                        mlog(ERROR, "Failed to process termination on %s for %s", key, dispatch.list[d]->getName());
                    }
                }
                key = dispatcher->dispatchTable.next(&dispatch);
            }

            /* Signal Completion */
            dispatcher->signalComplete();
        }
    }
    dispatcher->threadMut.unlock();

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
    catch(RunTimeException& e)
    {
        (void)e;
    }
}
