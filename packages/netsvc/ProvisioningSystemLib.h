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

#ifndef __provisioning_system_lib__
#define __provisioning_system_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define DEFAULT_ORGANIZATION_NAME   "sliderule"
#define DEFAULT_PS_URL              "https://ps.testsliderule.org"

/******************************************************************************
 * PROVISIONING SYSTEM LIBRARY CLASS
 ******************************************************************************/

class ProvisioningSystemLib
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init                (void);
        static void         deinit              (void);

        static bool         validate            (const char* access_token, bool verbose=false);

        static int          luaSetUrl           (lua_State* L);
        static int          luaSetOrganization  (lua_State* L);
        static int          luaValidate         (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* URL;
        static const char* Organization;

        /*--------------------------------------------------------------------
         * Authenticator Subclass
         *--------------------------------------------------------------------*/
        class Authenticator: public LuaEndpoint::Authenticator
        {
            public:
                static int luaCreate (lua_State* L);
                Authenticator(lua_State* L);
                ~Authenticator(void);
                bool isValid(const char* token) override;
        };
};

#endif  /* __provisioning_system_lib__ */
