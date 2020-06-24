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
