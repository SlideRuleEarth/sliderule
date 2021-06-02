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
#include "core.h"

#include <assert.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Mutex CredentialStore::credentialLock;
Dictionary<CredentialStore::Credential> CredentialStore::credentialStore(STARTING_STORE_SIZE);

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
CredentialStore::Credential CredentialStore::get (const char* host)
{
    Credential credential;

    credentialLock.lock();
    {
        try
        {
            credential = credentialStore[host];
        }
        catch(const RunTimeException& e)
        {
            (void)e;
        }
    }
    credentialLock.unlock();

    return credential;
}

/*----------------------------------------------------------------------------
 * put
 *----------------------------------------------------------------------------*/
bool CredentialStore::put (const char* host, Credential& credential)
{
    bool status = false;

    credentialLock.lock();
    {
        status = credentialStore.add(host, credential);
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
        const char* host = LuaObject::getLuaString(L, 1);

        /* Get Credentials */
        Credential credential = CredentialStore::get(host);

        /* Return Credentials */
        if(credential.access_key_id)
        {
            lua_newtable(L);

            lua_pushstring(L, "access_key_id");
            lua_pushstring(L, credential.access_key_id);
            lua_settable(L, -3);

            lua_pushstring(L, "secret_access_key");
            lua_pushstring(L, credential.secret_access_key);
            lua_settable(L, -3);

            lua_pushstring(L, "access_token");
            lua_pushstring(L, credential.access_token);
            lua_settable(L, -3);
 
            lua_pushstring(L, "expiration_time");
            lua_pushstring(L, credential.expiration_time);
            lua_settable(L, -3);

            return LuaObject::returnLuaStatus(L, true, 2);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error getting credential: %s", e.what());
    }

    return LuaObject::returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * luaPut - csput(<host>, <credential table>)
 *----------------------------------------------------------------------------*/
int CredentialStore::luaPut(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Host */
        const char* host = LuaObject::getLuaString(L, 1);

        /* Initialize Credentials */
        Credential credential;

        /* Get Credentials */
        int index = 2;
        if(lua_type(L, index) == LUA_TTABLE)
        {
            lua_getfield(L, index, "access_key_id");
            credential.access_key_id = StringLib::duplicate(LuaObject::getLuaString(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, index, "secret_access_key");
            credential.secret_access_key = StringLib::duplicate(LuaObject::getLuaString(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, index, "access_token");
            credential.access_token = StringLib::duplicate(LuaObject::getLuaString(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, index, "expiration_time");
            credential.expiration_time = StringLib::duplicate(LuaObject::getLuaString(L, -1));
            lua_pop(L, 1);
        }

        /* Put Credentials */
        status = CredentialStore::put(host, credential);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error putting credentials: %s", e.what());
    }

    return LuaObject::returnLuaStatus(L, status);
}
