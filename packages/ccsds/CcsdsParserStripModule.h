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

#ifndef __ccsds_parser_strip_module__
#define __ccsds_parser_strip_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsParserModule.h"
#include "CcsdsPacket.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PARSER STRIP MODULE CLASS
 ******************************************************************************/

class CcsdsParserStripModule: public CcsdsParserModule
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate       (lua_State* L);
        int         parseBuffer     (unsigned char* buffer, int bytes, CcsdsPacket* pkt); // returns number of bytes consumed
        void        gotoInitState   (bool reset);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*  LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            HDR,
            CCSDS
        } stream_state_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        stream_state_t  state;
        const int       HDR_SIZE;
        int             HdrBytes;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        CcsdsParserStripModule  (lua_State* L, int header_size);
        ~CcsdsParserStripModule (void);
};

#endif  /* __ccsds_parser_strip_module__ */
