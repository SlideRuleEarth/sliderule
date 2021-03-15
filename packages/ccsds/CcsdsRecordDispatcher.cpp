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

#include "CcsdsRecordDispatcher.h"
#include "CcsdsRecord.h"
#include "core.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - dispatcher(<input stream name>, [<num threads>], [<key mode>, <key parm>])
 *----------------------------------------------------------------------------*/
int CcsdsRecordDispatcher::luaCreate (lua_State* L)
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
            throw RunTimeException("invalid number of threads supplied (must be >= 1)");
        }

        /* Set Key Mode */
        keyMode_t key_mode = str2mode(key_mode_str);
        const char* key_field = NULL;
        calcFunc_t  key_func = NULL;
        if(key_mode == INVALID_KEY_MODE)
        {
            throw RunTimeException("Invalid key mode specified: %s\n", key_mode_str);
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
        return createLuaObject(L, new CcsdsRecordDispatcher(L, qname, key_mode, key_field, key_func, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
    catch(std::out_of_range& e)
    {
        (void)e;
        mlog(CRITICAL, "Invalid calculation function provided - no handler installed\n");
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsRecordDispatcher::CcsdsRecordDispatcher(lua_State* L, const char* inputq_name, keyMode_t key_mode, const char* key_field, calcFunc_t key_func, int num_threads):
    RecordDispatcher(L, inputq_name, key_mode, key_field, key_func, num_threads, MsgQ::SUBSCRIBER_OF_CONFIDENCE)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsRecordDispatcher::~CcsdsRecordDispatcher(void)
{
}

/*----------------------------------------------------------------------------
 * createRecord
 *----------------------------------------------------------------------------*/
RecordObject* CcsdsRecordDispatcher::createRecord (unsigned char* buffer, int size)
{
    return new CcsdsRecordInterface(buffer, size);
}
