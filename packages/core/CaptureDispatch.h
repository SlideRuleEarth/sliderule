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
