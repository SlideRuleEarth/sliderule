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

#ifndef __ccsds_payload_dispatch__
#define __ccsds_payload_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "DispatchObject.h"
#include "CcsdsRecord.h"
#include "CcsdsPacket.h"
#include "Dictionary.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PAYLOAD DISPATCH
 ******************************************************************************/

class CcsdsPayloadDispatch: public DispatchObject
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

        static int  luaCreate       (lua_State* L);
        bool        processRecord   (RecordObject* record, okey_t key);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<Publisher*>  qLookUp; // qLookUp[qname] ==> Publisher* , this prevents multiple publishers to same queue
        Publisher*              outQ[CCSDS_NUM_APIDS];
        Mutex                   qMut;
        bool                    checkLength;
        bool                    checkChecksum;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    CcsdsPayloadDispatch    (lua_State* L);
                    ~CcsdsPayloadDispatch   (void);

        void        setPublisher            (int apid, const char* qname);

        static int  luaForwardPacket        (lua_State* L);
        static int  luaCheckLength          (lua_State* L);
        static int  luaCheckChecksum        (lua_State* L);
};

#endif  /* __ccsds_payload_dispatch__ */
