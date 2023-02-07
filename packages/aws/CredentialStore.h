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

#ifndef __credential_store__
#define __credential_store__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "LuaEngine.h"
#include "LuaObject.h"
#include "TimeLib.h"

/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/

class CredentialStore
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int STARTING_STORE_SIZE = 8;
        static const int MAX_KEY_SIZE = 2048;

        static const char* LIBRARY_NAME;
        static const char* EXPIRATION_GPS_METRIC;

        // Baseline EarthData Login Keys
        static const char* ACCESS_KEY_ID_STR;
        static const char* SECRET_ACCESS_KEY_STR;
        static const char* SESSION_TOKEN_STR;
        static const char* EXPIRATION_STR;

        // AWS MetaData Keys
        static const char* ACCESS_KEY_ID_STR1;
        static const char* SECRET_ACCESS_KEY_STR1;
        static const char* SESSION_TOKEN_STR1;
        static const char* EXPIRATION_STR1;

        // AWS Credential File Keys
        static const char* ACCESS_KEY_ID_STR2;
        static const char* SECRET_ACCESS_KEY_STR2;
        static const char* SESSION_TOKEN_STR2;

        /*--------------------------------------------------------------------
         * Typdefs
         *--------------------------------------------------------------------*/

        struct Credential {
            bool        provided;
            const char* accessKeyId;
            const char* secretAccessKey;
            const char* sessionToken;
            const char* expiration;
            int64_t     expirationGps;

            Credential(void) {
                zero();
            };

            Credential(const Credential& c) {
                copy(c);
            };

            const Credential& operator=(const Credential& c) {
                cleanup();
                copy(c);
                return *this;
            };

            ~Credential(void) {
                cleanup();
            };

            void fromLua (lua_State* L, int index)
            {
                if(lua_type(L, index) == LUA_TTABLE)
                {
                    /* Set Provided */
                    provided = true;

                    /* Get Access Key */
                    if(lua_getfield(L, index, ACCESS_KEY_ID_STR) != LUA_TSTRING)
                    {
                        lua_pop(L, 1);
                        if(lua_getfield(L, index, ACCESS_KEY_ID_STR1) != LUA_TSTRING)
                        {
                            lua_pop(L, 1);
                            lua_getfield(L, index, ACCESS_KEY_ID_STR2);
                        }
                    }
                    const char* access_key_id_str = LuaObject::getLuaString(L, -1);
                    accessKeyId = StringLib::duplicate(access_key_id_str, MAX_KEY_SIZE);
                    lua_pop(L, 1);

                    /* Get Secret Access Key */
                    if(lua_getfield(L, index, SECRET_ACCESS_KEY_STR) != LUA_TSTRING)
                    {
                        lua_pop(L, 1);
                        if(lua_getfield(L, index, SECRET_ACCESS_KEY_STR1) != LUA_TSTRING)
                        {
                            lua_pop(L, 1);
                            lua_getfield(L, index, SECRET_ACCESS_KEY_STR2);
                        }
                    }
                    const char* secret_access_key_str = LuaObject::getLuaString(L, -1);
                    secretAccessKey = StringLib::duplicate(secret_access_key_str, MAX_KEY_SIZE);
                    lua_pop(L, 1);

                    /* Get Session Token */
                    if(lua_getfield(L, index, SESSION_TOKEN_STR) != LUA_TSTRING)
                    {
                        lua_pop(L, 1);
                        if(lua_getfield(L, index, SESSION_TOKEN_STR1) != LUA_TSTRING)
                        {
                            lua_pop(L, 1);
                            lua_getfield(L, index, SESSION_TOKEN_STR2);
                        }
                    }
                    const char* session_token_str = LuaObject::getLuaString(L, -1);
                    sessionToken = StringLib::duplicate(session_token_str, MAX_KEY_SIZE);
                    lua_pop(L, 1);

                    /* Get Expiration Date */
                    if(lua_getfield(L, index, EXPIRATION_STR) != LUA_TSTRING)
                    {
                        lua_pop(L, 1);
                        lua_getfield(L, index, EXPIRATION_STR1);
                    }
                    const char* expiration_str  = LuaObject::getLuaString(L, -1, true, NULL);
                    expiration = StringLib::duplicate(expiration_str, MAX_KEY_SIZE);
                    if(expiration)  expirationGps = TimeLib::str2gpstime(expiration);
                    else            expirationGps = 0;
                    lua_pop(L, 1);
                }
            };

            void toLua (lua_State* L)
            {
                lua_newtable(L);

                lua_pushstring(L, ACCESS_KEY_ID_STR);
                lua_pushstring(L, accessKeyId);
                lua_settable(L, -3);

                lua_pushstring(L, SECRET_ACCESS_KEY_STR);
                lua_pushstring(L, secretAccessKey);
                lua_settable(L, -3);

                lua_pushstring(L, SESSION_TOKEN_STR);
                lua_pushstring(L, sessionToken);
                lua_settable(L, -3);

                lua_pushstring(L, EXPIRATION_STR);
                lua_pushstring(L, expiration);
                lua_settable(L, -3);
            };

            private:

                void zero (void) {
                    provided = false;
                    accessKeyId = NULL;
                    secretAccessKey = NULL;
                    sessionToken = NULL;
                    expiration = NULL;
                    expirationGps = 0L;
                }

                void cleanup (void) {
                    if(accessKeyId) delete [] accessKeyId;
                    if(secretAccessKey) delete [] secretAccessKey;
                    if(sessionToken) delete [] sessionToken;
                    if(expiration) delete [] expiration;
                }

                void copy (const Credential& c) {
                    provided = c.provided;
                    accessKeyId = StringLib::duplicate(c.accessKeyId, MAX_KEY_SIZE);
                    secretAccessKey = StringLib::duplicate(c.secretAccessKey, MAX_KEY_SIZE);
                    sessionToken = StringLib::duplicate(c.sessionToken, MAX_KEY_SIZE);
                    expiration = StringLib::duplicate(c.expiration, MAX_KEY_SIZE);
                    expirationGps = c.expirationGps;
                }
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init    (void);
        static void         deinit  (void);

        static Credential   get     (const char* host);
        static bool         put     (const char* host, Credential& credential);

        static int          luaGet  (lua_State* L);
        static int          luaPut  (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex credentialLock;
        static Dictionary<Credential> credentialStore;
        static Dictionary<int32_t> metricIds;
};

#endif  /* __credential_store__ */
