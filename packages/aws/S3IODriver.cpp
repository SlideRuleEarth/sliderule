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

#include "S3IODriver.h"
#include "OsApi.h"
#include "Asset.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/model/GetObjectRequest.h>

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

struct S3IODriver::impl
{
    Aws::S3Crt::S3CrtClient* s3_client;
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3IODriver::FORMAT = "s3";
const char* S3IODriver::DEFAULT_REGION = "us-west-2";
const char* S3IODriver::DEFAULT_ENDPOINT = "https://s3.us-west-2.amazonaws.com";

/******************************************************************************
 * FILE IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* S3IODriver::create (const Asset* _asset, const char* resource)
{
    return new S3IODriver(_asset, resource);
}

/*----------------------------------------------------------------------------
 * ioRead
 *----------------------------------------------------------------------------*/
int64_t S3IODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    int64_t bytes_read = 0;
    Aws::S3Crt::Model::GetObjectRequest object_request;

    /* Set Bucket and Key */
    object_request.SetBucket(ioBucket);
    object_request.SetKey(ioKey);

    /* Set Range */
    SafeString s3_rqst_range("bytes=%lu-%lu", (unsigned long)pos, (unsigned long)(pos + size - 1));
    object_request.SetRange(s3_rqst_range.getString());

    /* Make Request */
    Aws::S3Crt::Model::GetObjectOutcome response = pimpl->s3_client->GetObject(object_request);
    bool status = response.IsSuccess();

    /* Read Response */
    if(status)
    {
        bytes_read = (int64_t)response.GetResult().GetContentLength();
        std::streambuf* sbuf = response.GetResult().GetBody().rdbuf();
        std::istream reader(sbuf);
        reader.read((char*)data, bytes_read);
    }

    /* Handle Errors or Return Bytes Read */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read S3 data: %s", response.GetError().GetMessage().c_str());
    }

    return bytes_read;
}

/*----------------------------------------------------------------------------
 * luaGet - s3get(<bucket>, <key>, [<region>], [<endpoint>]) -> contents
 *----------------------------------------------------------------------------*/
int S3IODriver::luaGet(lua_State* L)
{
    bool status = false;
    int num_rets = 1;
    Aws::S3Crt::S3CrtClient* s3_client = NULL;

    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        const char* region      = LuaObject::getLuaString(L, 3, true, DEFAULT_REGION);
        const char* endpoint    = LuaObject::getLuaString(L, 4, true, DEFAULT_ENDPOINT);

        /* Initialize Variables for Read */
        char* data = NULL;
        int64_t bytes_read = 0;
        Aws::S3Crt::Model::GetObjectRequest object_request;

        /* Set Bucket and Key */
        object_request.SetBucket(bucket);
        object_request.SetKey(key);

        /* Create S3 Client */
        Aws::S3Crt::ClientConfiguration client_config;
        client_config.endpointOverride = endpoint;
        client_config.region = region;
        s3_client = new Aws::S3Crt::S3CrtClient(client_config);

        /* Make Request */
        Aws::S3Crt::Model::GetObjectOutcome response = s3_client->GetObject(object_request);
        if(response.IsSuccess())
        {
            bytes_read = (int64_t)response.GetResult().GetContentLength();
            data = new char [bytes_read];
            std::streambuf* sbuf = response.GetResult().GetBody().rdbuf();
            std::istream reader(sbuf);
            reader.read((char*)data, bytes_read);

            /* Push Contents */
            lua_pushlstring(L, data, bytes_read);
            status = true;
            num_rets++;
        }
        else
        {
            mlog(CRITICAL, "Http error getting S3 object: %s", response.GetError().GetMessage().c_str());
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting S3 object: %s", e.what());
    }

    /* Free S3 Client */
    if(s3_client) delete s3_client;

    /* Return Results */
    lua_pushboolean(L, status);
    return num_rets;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
S3IODriver::S3IODriver (const Asset* _asset, const char* resource):
    asset(_asset)
{
    SafeString resourcepath("%s/%s", asset->getPath(), resource);

    /* Allocate Memory */
    ioBucket = StringLib::duplicate(resourcepath.getString());

    /*
    * Differentiate Bucket and Key
    *  <bucket_name>/<path_to_file>/<filename>
    *  |             |
    * ioBucket      ioKey
    */
    ioKey = ioBucket;
    while(*ioKey != '\0' && *ioKey != '/') ioKey++;
    if(*ioKey == '/') *ioKey = '\0';
    else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid S3 url: %s", resource);
    ioKey++;

    /* Get Latest Credentials */
    CredentialStore::Credential latest_credential = CredentialStore::get(asset->getName());

    /* Create S3 Client Configuration */
    Aws::S3Crt::ClientConfiguration client_config;
    client_config.region = asset->getRegion();
    client_config.endpointOverride = asset->getEndpoint();

    /* Create S3 Client */
    pimpl = new S3IODriver::impl;
    if(latest_credential.provided)
    {
        const Aws::String accessKeyId(latest_credential.accessKeyId);
        const Aws::String secretAccessKey(latest_credential.secretAccessKey);
        const Aws::String sessionToken(latest_credential.sessionToken);
        Aws::Auth::AWSCredentials awsCredentials(accessKeyId, secretAccessKey, sessionToken);
        pimpl->s3_client = new Aws::S3Crt::S3CrtClient(awsCredentials, client_config);
    }
    else
    {
        pimpl->s3_client = new Aws::S3Crt::S3CrtClient(client_config);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3IODriver::~S3IODriver (void)
{
    delete pimpl->s3_client;
    delete pimpl;

    /*
     * Delete Memory Allocated for ioBucket
     *  only ioBucket is freed because ioKey only points
     *  into the memory allocated to ioBucket
     */
    if(ioBucket)
    {
        delete [] ioBucket;
        ioBucket = NULL;
    }
}
