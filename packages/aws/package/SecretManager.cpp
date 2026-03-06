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

#include <aws/core/Aws.h>
#include <aws/secretsmanager/SecretsManagerClient.h>
#include <aws/secretsmanager/model/GetSecretValueRequest.h>
#include <rapidjson/document.h>

#include "SecretManager.h"
#include "OsApi.h"
#include "EventLib.h"
#include "LuaObject.h"
#include "StringLib.h"
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SecretManager::PUBKEYS_SECRET = "pubkeys";
Mutex SecretManager::secretCacheMutex;
Dictionary<string> SecretManager::secretCache;

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * get - "{domain}/<secret_name>"[<key>] --> new char[]
 *----------------------------------------------------------------------------*/
string SecretManager::get(const char* secret_name, const char* key)
{
    // check cache for secret
    FString cached_secret("%s/%s/%s", SystemConfig::settings().domain.value.c_str(), secret_name, key);
    secretCacheMutex.lock();
    {
        string value;
        if(secretCache.find(cached_secret.c_str(), &value))
        {
            secretCacheMutex.unlock();
            return value;
        }
    }
    secretCacheMutex.unlock();

    // setup AWS secrets manager client
    FString aws_secret("%s/%s", SystemConfig::settings().domain.value.c_str(), secret_name);
    Aws::SecretsManager::SecretsManagerClient client;
    Aws::SecretsManager::Model::GetSecretValueRequest request;
    request.SetSecretId(aws_secret.c_str());

    // fetch secret from AWS secrets manager
    auto outcome = client.GetSecretValue(request);
    if(outcome.IsSuccess())
    {
        // process non-empty secret value
        const auto& result = outcome.GetResult();
        if(!result.GetSecretString().empty())
        {
            try
            {
                // parse contents as json
                const char* secret_str = result.GetSecretString().c_str();
                rapidjson::Document json;
                json.Parse(secret_str);

                // check membership
                if(json.HasMember(key))
                {
                    string value = json[key].GetString();
                    if(!value.empty())
                    {
                        // cache value
                        secretCacheMutex.lock();
                        {
                            if(!secretCache.add(cached_secret.c_str(), value))
                            {
                                mlog(ERROR, "Failed to cache secret: %s", cached_secret.c_str());
                            }
                        }
                        secretCacheMutex.unlock();

                        // success; return secret value
                        return value;
                    }
                    else
                    {
                        mlog(CRITICAL, "Failed to retrieve <%s> from <%s>", key, aws_secret.c_str());
                    }
                }
                else
                {
                    mlog(CRITICAL, "Secret not present for key <%s>", key);
                }
            }
            catch(const std::exception& e)
            {
                mlog(CRITICAL, "Failed to parse secret: %s", e.what());
            }
        }
        else
        {
            mlog(CRITICAL, "Secret <%s> is empty", aws_secret.c_str());
        }
    }
    else
    {
        const auto& error = outcome.GetError();
        mlog(CRITICAL, "Failed to retrieve secret <%s>: %s - %s", aws_secret.c_str(), error.GetExceptionName().c_str(), error.GetMessage().c_str());
    }

    // failure; return empty string
    return string();
}

/*----------------------------------------------------------------------------
 * luaGet - secret(<name>, <key>) --> value
 *----------------------------------------------------------------------------*/
int SecretManager::luaGet(lua_State* L)
{
    try
    {
        const char* secret_name = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        string secret_value     = get(secret_name, key);

        if(!secret_value.empty()) lua_pushstring(L, secret_value.c_str());
        else lua_pushnil(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting secret: %s", e.what());
    }

    return 1;
}
