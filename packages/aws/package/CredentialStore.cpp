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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/


#include "CredentialStore.h"
#include "OsApi.h"

#include <assert.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Mutex CredentialStore::credentialLock;
Dictionary<CredentialStore::Credential> CredentialStore::credentialStore(STARTING_STORE_SIZE);
CredentialStore::Credential CredentialStore::emptyCredentials;

const char* CredentialStore::LIBRARY_NAME = "CredentialStore";
const char* CredentialStore::EXPIRATION_GPS_METRIC = "exp_gps";

const char* CredentialStore::ACCESS_KEY_ID_STR = "accessKeyId";
const char* CredentialStore::SECRET_ACCESS_KEY_STR = "secretAccessKey";
const char* CredentialStore::SESSION_TOKEN_STR = "sessionToken";
const char* CredentialStore::EXPIRATION_STR = "expiration";

const char* CredentialStore::ACCESS_KEY_ID_STR1 = "AccessKeyId";
const char* CredentialStore::SECRET_ACCESS_KEY_STR1 = "SecretAccessKey";
const char* CredentialStore::SESSION_TOKEN_STR1 = "Token";
const char* CredentialStore::EXPIRATION_STR1 = "Expiration";

const char* CredentialStore::ACCESS_KEY_ID_STR2 = "aws_access_key_id";
const char* CredentialStore::SECRET_ACCESS_KEY_STR2 = "aws_secret_access_key";
const char* CredentialStore::SESSION_TOKEN_STR2 = "aws_session_token";

/******************************************************************************
 * CREDENTIAL STORE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void CredentialStore::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void CredentialStore::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
const CredentialStore::Credential& CredentialStore::get (const char* identity)
{
    credentialLock.lock();
    {
        try
        {
            const Credential& credential = credentialStore[identity];
            credentialLock.unlock();
            return credential;
        }
        catch(const RunTimeException& e)
        {
            (void)e;
        }
    }
    credentialLock.unlock();

    return emptyCredentials;
}

/*----------------------------------------------------------------------------
 * put
 *----------------------------------------------------------------------------*/
bool CredentialStore::put (const char* identity, const Credential& credential)
{
    bool status = false;

    credentialLock.lock();
    {
        /* Store Credentials */
        status = credentialStore.add(identity, credential);
    }
    credentialLock.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * luaGet - csget(<host>)
 *----------------------------------------------------------------------------*/
int CredentialStore::luaGet(lua_State* L)
{
    try
    {
        /* Get Host */
        const char* identity = LuaObject::getLuaString(L, 1);

        /* Get Credentials */
        const Credential& credential = CredentialStore::get(identity);

        /* Return Credentials */
        if(!credential.expiration.value.empty())
        {
            return credential.toLua(L);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting credential: %s", e.what());
    }

    lua_pushnil(L);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaPut - csput(<asset>, <credential table>)
 *----------------------------------------------------------------------------*/
int CredentialStore::luaPut(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Host */
        const char* asset = LuaObject::getLuaString(L, 1);

        /* Get Credentials */
        Credential credential;
        if(lua_istable(L, 2))
        {
            credential.fromLua(L, 2);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "must supply table for credentials");
        }
        /* Put Credentials */
        status = CredentialStore::put(asset, credential);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error putting credential: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}
