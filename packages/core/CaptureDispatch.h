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

#ifndef __capture_dispatch__
#define __capture_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "DispatchObject.h"
#include "OsApi.h"
#include "List.h"
#include "RecordObject.h"
#include "MsgQ.h"

/******************************************************************************
 * CAPTURE DISPATCH
 ******************************************************************************/

class CaptureDispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        struct capture_t
        {
            bool            filter_id;
            long            id;
            Cond            cond;
            int             timeout;
            const char*     field_name;

            capture_t(bool _filter_id, long _id, const char* _field_str, int _timeout)
                { filter_id = _filter_id;
                  id = _id;
                  field_name = StringLib::duplicate(_field_str);
                  timeout = _timeout; }
            ~capture_t(void)
                { if(field_name) delete [] field_name; }
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        MgList<capture_t*>  captures;
        Mutex               capMut;
        Publisher*          outQ;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    CaptureDispatch     (lua_State* L, const char* outq_name);
                    ~CaptureDispatch    (void);

        static void freeCaptureEntry    (void* obj, void* parm);

        /* overridden methods */
        bool        processRecord       (RecordObject* record, okey_t key);

        /* meta functions */
        static int  luaCapture          (lua_State* L);
        static int  luaClear            (lua_State* L);
        static int  luaRemove           (lua_State* L);
};

#endif  /* __capture_dispatch__ */
