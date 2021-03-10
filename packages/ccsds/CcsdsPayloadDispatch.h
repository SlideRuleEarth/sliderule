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
