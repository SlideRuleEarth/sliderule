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

#include "SecretManager.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
const char* SecretManager::get(const char* secret_name)
{
    string value;

    Aws::SecretsManager::SecretsManagerClient client;
    Aws::SecretsManager::Model::GetSecretValueRequest request;
    request.SetSecretId(secret_name);

    auto outcome = client.GetSecretValue(request);
    if(outcome.IsSuccess())
    {
        const auto& result = outcome.GetResult();
        if(!result.GetSecretString().empty())
        {
            return result.GetSecretString().c_str();
        }
        else
        {
            mlog(CRITICAL, "Secret <%s> is empty", secret_name);
        }
    }
    else
    {
        const auto& error = outcome.GetError();
        mlog(CRITICAL, "Failed to retrieve secret <%s>: %s - %s", secret_name, error.GetExceptionName().c_str(), error.GetMessage().c_str());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaGet - secret(<name>) --> value
 *----------------------------------------------------------------------------*/
int SecretManager::luaGet(lua_State* L)
{
    try
    {
        const char* secret_name = LuaObject::getLuaString(L, 1);
        const char* secret_value = get(secret_name);
        if(secret_value)    lua_pushstring(L, secret_value);
        else                lua_pushnil(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting secret: %s", e.what());
    }

    return 1;
}
