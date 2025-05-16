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
#include "FieldElement.h"
#include "FieldDictionary.h"
#include "LuaObject.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class CredentialStore
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int STARTING_STORE_SIZE = 8;

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

        struct Credential: public FieldDictionary {
            FieldElement<string>    accessKeyId;
            FieldElement<string>    secretAccessKey;
            FieldElement<string>    sessionToken;
            FieldElement<string>    expiration;

            string toJson(void) const override {
                return "{}"; // do not export credentials for security reasons
            };

            Credential(void): FieldDictionary({
                {ACCESS_KEY_ID_STR, &accessKeyId},
                {ACCESS_KEY_ID_STR1, &accessKeyId},
                {ACCESS_KEY_ID_STR2, &accessKeyId},

                {SECRET_ACCESS_KEY_STR, &secretAccessKey},
                {SECRET_ACCESS_KEY_STR1, &secretAccessKey},
                {SECRET_ACCESS_KEY_STR2, &secretAccessKey},

                {SESSION_TOKEN_STR, &sessionToken},
                {SESSION_TOKEN_STR1, &sessionToken},
                {SESSION_TOKEN_STR2, &sessionToken},

                {EXPIRATION_STR, &expiration},
                {EXPIRATION_STR1, &expiration}
            }) {};

            Credential(const Credential& c): FieldDictionary({
                {ACCESS_KEY_ID_STR, &accessKeyId},
                {ACCESS_KEY_ID_STR1, &accessKeyId},
                {ACCESS_KEY_ID_STR2, &accessKeyId},

                {SECRET_ACCESS_KEY_STR, &secretAccessKey},
                {SECRET_ACCESS_KEY_STR1, &secretAccessKey},
                {SECRET_ACCESS_KEY_STR2, &secretAccessKey},

                {SESSION_TOKEN_STR, &sessionToken},
                {SESSION_TOKEN_STR1, &sessionToken},
                {SESSION_TOKEN_STR2, &sessionToken},

                {EXPIRATION_STR, &expiration},
                {EXPIRATION_STR1, &expiration}
            }) {
                accessKeyId = c.accessKeyId;
                secretAccessKey = c.secretAccessKey;
                sessionToken = c.sessionToken;
                expiration = c.expiration;
            };

            Credential& operator=(const Credential& c) {
                if(this != &c) {
                    accessKeyId = c.accessKeyId;
                    secretAccessKey = c.secretAccessKey;
                    sessionToken = c.sessionToken;
                    expiration = c.expiration;
                }

                return *this;
            };

            ~Credential(void) override = default;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void                 init    (void);
        static void                 deinit  (void);

        static const Credential&    get     (const char* identity);
        static bool                 put     (const char* identity, const Credential& credential);

        static int                  luaGet  (lua_State* L);
        static int                  luaPut  (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex credentialLock;
        static Dictionary<Credential> credentialStore;
        static Credential emptyCredentials;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

inline uint32_t toEncoding(CredentialStore::Credential& v) { (void)v; return Field::USER; }

#endif  /* __credential_store__ */
