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
Dictionary<int32_t> CredentialStore::metricIds(STARTING_STORE_SIZE);

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
        /* Store Credentials */
        status = credentialStore.add(host, credential);

        /* Find/Register Metric Id */
        int32_t metric_id = EventLib::INVALID_METRIC;
        if(!metricIds.find(host, &metric_id))
        {
            metric_id = EventLib::registerMetric(LIBRARY_NAME, EventLib::GAUGE, "%s:%s", host ? host : "<unknown>", EXPIRATION_GPS_METRIC);
            metricIds.add(host, metric_id);
        }

        /* Update Metric */
        if(metric_id != EventLib::INVALID_METRIC)
        {
            if(credential.expiration != NULL)
            {
                update_metric(DEBUG, metric_id, credential.expirationGps);
            }
            else
            {
                mlog(CRITICAL, "Null expiration time supplied to credential for %s", host);
            }
        }
        else
        {
            mlog(CRITICAL, "Unable to register credential metric for %s", host);
        }
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
        if(credential.provided)
        {
            credential.toLua(L);
            lua_pushboolean(L, true);
            return 2;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting credential: %s", e.what());
    }

    lua_pushboolean(L, false);
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
        credential.fromLua(L, 2);
        if(!credential.provided)
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
