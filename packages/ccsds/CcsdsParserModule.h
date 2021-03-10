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
