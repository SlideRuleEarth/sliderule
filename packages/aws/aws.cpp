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
 *INCLUDES
 ******************************************************************************/

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>

#include "core.h"
#include "aws.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_AWS_LIBNAME         "aws"

#define AWS_DEFAULT_REGION      "us-west-2"
#define AWS_DEFAULT_ENDPOINT    "https://s3.us-west-2.amazonaws.com"

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

Aws::SDKOptions options;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * aws_s3_get - s3get(<bucket>, <key>, [<region>], [<endpoint>]) -> contents
 *----------------------------------------------------------------------------*/
int aws_s3_get(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        const char* region      = LuaObject::getLuaString(L, 3, true, AWS_DEFAULT_REGION);
        const char* endpoint    = LuaObject::getLuaString(L, 4, true, AWS_DEFAULT_ENDPOINT);

        /* Initialize Variables for Read */
        char* data = NULL;
        int64_t bytes_read = 0;
        Aws::S3::Model::GetObjectRequest object_request;

        /* Set Bucket and Key */
        object_request.SetBucket(bucket);
        object_request.SetKey(key);

        /* Create S3 Client */
        Aws::Client::ClientConfiguration client_config;
        client_config.endpointOverride = endpoint;
        client_config.region = region;
        Aws::S3::S3Client* s3_client = new Aws::S3::S3Client(client_config);

        /* Make Request */
        Aws::S3::Model::GetObjectOutcome response = s3_client->GetObject(object_request);
        if(response.IsSuccess())
        {
            bytes_read = (int64_t)response.GetResult().GetContentLength();
            data = new char [bytes_read];
            std::streambuf* sbuf = response.GetResult().GetBody().rdbuf();
            std::istream reader(sbuf);
            reader.read((char*)data, bytes_read);

            /* Return Contents */
            lua_pushlstring(L, data, bytes_read);
            lua_pushboolean(L, true);
            return 2;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to get S3 object");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting S3 object: %s", e.what());
    }

    lua_pushboolean(L, false);
    return 1;
}

/*----------------------------------------------------------------------------
 * aws_open
 *----------------------------------------------------------------------------*/
int aws_open (lua_State *L)
{
    static const struct luaL_Reg aws_functions[] = {
        {"csget",       CredentialStore::luaGet},
        {"csput",       CredentialStore::luaPut},
        {"s3get",       aws_s3_get},
        {"s3cache",     S3CacheIODriver::luaCreateCache},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, aws_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initaws (void)
{
    /* Configure AWS Logging */
    #ifdef ENABLE_AWS_LOGGING
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
    #endif

    /* Configure AWS to Handle Broken Pipes */
    options.httpOptions.installSigPipeHandler = true;

    /* Initialize AWS SDK */
    Aws::InitAPI(options);

    /* Initialize Modules */
    CredentialStore::init();
    S3Lib::init();
    Asset::registerDriver(S3IODriver::FORMAT, S3IODriver::create);
    Asset::registerDriver(S3CacheIODriver::FORMAT, S3CacheIODriver::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_AWS_LIBNAME, aws_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_AWS_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_AWS_LIBNAME, LIBID);
}

void deinitaws (void)
{
    S3Lib::deinit();
    CredentialStore::deinit();
    Aws::ShutdownAPI(options);
}
}
