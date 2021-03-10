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

#ifndef __record_dispatcher__
#define __record_dispatcher__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "DispatchObject.h"
#include "LuaObject.h"
#include "Dictionary.h"
#include "List.h"
#include "MsgQ.h"
#include "RecordObject.h"
#include "OsApi.h"

/******************************************************************************
 * RECORD DISPATCHER CLASS
 ******************************************************************************/

class RecordDispatcher: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef okey_t (*calcFunc_t) (unsigned char* buffer, int size);

        typedef enum {
            FIELD_KEY_MODE = 0,
            RECEIPT_KEY_MODE = 1,
            CALCULATED_KEY_MODE = 2,
            INVALID_KEY_MODE = 3
        } keyMode_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DISPATCH_TIMEOUT = 1000; // milliseconds

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);

        static keyMode_t    str2mode        (const char* str);
        static const char*  mode2str        (keyMode_t mode);
        static bool         addKeyCalcFunc  (const char* calc_name, calcFunc_t func);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Dictionary<calcFunc_t>   keyCalcFunctions;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                RecordDispatcher    (lua_State* L, const char* inputq_name, keyMode_t key_mode, const char* key_field, calcFunc_t key_func, int num_threads);
        virtual                 ~RecordDispatcher   (void);
        virtual RecordObject*   createRecord        (unsigned char* buffer, int size);

        static int              luaRun              (lua_State* L);
        static int              luaAttachDispatch   (lua_State* L);
        static int              luaClearError       (lua_State* L);
        static int              luaDrain            (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            DispatchObject**    list;
            int                 size;
        } dispatch_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                    dispatcherActive;
        Thread**                threadPool;
        int                     numThreads;
        int                     threadsComplete;
        Mutex                   threadMut;
        Subscriber*             inQ;
        List<DispatchObject*>   dispatchList;   // for processTimeout
        Dictionary<dispatch_t>  dispatchTable;  // for processRecord
        Mutex                   dispatchMutex;
        keyMode_t               keyMode;        // determines key of metric
        okey_t                  keyRecCnt;      // used with RECEIPT_KEY_MODE
        const char*             keyField;       // used with FIELD_KEY_MODE
        calcFunc_t              keyFunc;        // used with CALCULATED_KEY_MODE
        bool                    recError;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    dispatcherThread    (void* parm);
        void            dispatchRecord      (RecordObject* record);

        void            startThreads        (void);
        void            stopThreads         (void);
};

#endif  /* __record_dispatcher__ */
