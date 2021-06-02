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

        /*--------------------------------------------------------------------
         * Typdefs
         *--------------------------------------------------------------------*/

        struct Credential {
            const char* access_key_id;
            const char* secret_access_key;
            const char* access_token;
            const char* expiration_time;

            Credential(void) {
                zero();
            };

            Credential(const Credential& c) {
                cleanup();
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

            private:

                void zero (void) {
                    access_key_id = NULL;
                    secret_access_key = NULL;
                    access_token = NULL;
                    expiration_time = NULL;
                }

                void cleanup (void) {
                    if(access_key_id) delete [] access_key_id;
                    if(secret_access_key) delete [] secret_access_key;
                    if(access_token) delete [] access_token;
                    if(expiration_time) delete [] expiration_time;
                }

                void copy (const Credential& c) {
                    access_key_id = StringLib::duplicate(c.access_key_id);
                    secret_access_key = StringLib::duplicate(c.secret_access_key);
                    access_token = StringLib::duplicate(c.access_token);
                    expiration_time = StringLib::duplicate(c.expiration_time);
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
};

#endif  /* __credential_store__ */
