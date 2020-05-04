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

#ifndef __ccsds_parser_module__
#define __ccsds_parser_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsPacket.h"
#include "LuaObject.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PARSER MODULE
 ******************************************************************************/

class CcsdsParserModule: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*  OBJECT_TYPE;
        static const int    PARSE_ERROR = -1;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate       (lua_State* L);

        virtual int     parseBuffer     (unsigned char* buffer, int bytes, CcsdsPacket* pkt); // returns number of bytes consumed
        virtual void    gotoInitState   (bool reset);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        CcsdsParserModule   (lua_State* L);
        CcsdsParserModule   (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
        ~CcsdsParserModule  (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*  LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];
};

#endif  /* __ccsds_parser_module__ */
