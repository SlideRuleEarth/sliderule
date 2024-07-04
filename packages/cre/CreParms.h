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

#ifndef __cre_parms__
#define __cre_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"

/******************************************************************************
 * CRE PARAMETERS CLASS
 ******************************************************************************/

class CreParms: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const char* SELF;
        static const char* PARMS;
        static const char* IMAGE;
        static const char* NAME;
        static const char* COMMAND;
        static const char* TIMEOUT;

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const int DEFAULT_TIMEOUT = 600;

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        const char* image; // container image
        const char* name; // container name
        const char* command; // container command
        int timeout; // on requests to docker daemon

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int  luaCreate           (lua_State* L);
                    CreParms            (lua_State* L, int index);
                    ~CreParms           (void) override;
        const char* tojson              (void) const override;

    private:

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void            cleanup             (void);
        static int      luaImage            (lua_State* L);
};

#endif  /* __cre_parms__ */
