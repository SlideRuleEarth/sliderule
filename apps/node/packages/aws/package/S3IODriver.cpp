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
#include "CredentialStore.h"
#include "SystemConfig.h"
#include "OsApi.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3IODriver::DEFAULT_IDENTITY = "iam-role";
const char* S3IODriver::DRIVER_FORMAT = "aws";

/******************************************************************************
 * AWS S3 I/O DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3IODriver::init (void)
{
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* S3IODriver::create (const Asset* _asset, const char* resource)
{
    /* Get Credentials */
    CredentialStore::Credential c = CredentialStore::get(_asset->getIdentity());
    Aws::Auth::AWSCredentials credentials(c.accessKeyId.value.c_str(), c.secretAccessKey.value.c_str(), c.sessionToken.value.c_str());

    /* Get AWS Client Configuration */
    Aws::Client::ClientConfiguration config;

    /*
    * Get Bucket and Key
    *  <bucket_name>/<path_to_file>/<filename>
    *  |             |
    * ioBucket      ioKey
    */
    FString resourcepath("%s/%s", asset->getPath(), resource);
    const char* bucket = resourcepath.c_str();
    char* key = StringLib::find(bucket, '/');
    if((!key) || ((bucket - key) < 0) || ((bucket - key) > resourcepath.length()))
    {
        RunTimeException(CRITICAL, RTE_FAILURE, "invalid s3 url: %s", resource);
    }
    const long t = key - bucket; // get index of first '/'
    resourcepath[t] = '\0'; // terminate at first '/'
    key++; // move key to character after first '/'

    /* Create Driver*/
    return new S3IODriver(bucket, key, credentials, config);
}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
int64_t S3IODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    if(size <= 0) return 0;
    FString range("bytes=%d-%d", pos, pos + size - 1)
    request.SetRange(range.c_str());
    auto outcome = client.GetObject(request);
    if(!outcome.IsSuccess())
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to make range request of %ld bytes at 0x%x: %s", size, pos, outcome.GetError().GetMessage());
    }
    stream.read(data, size);
    return stream.gcount();
}

/*----------------------------------------------------------------------------
 * path
 *----------------------------------------------------------------------------*/
string S3IODriver::path (void)
{
    return "";
}

/*----------------------------------------------------------------------------
 * size
 *----------------------------------------------------------------------------*/
int64_t S3IODriver::size (void)
{
    auto outcome = client.HeadObject(request);
    if (!outcome.IsSuccess())
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to get size of object: %s", outcome.GetError().GetMessage());
    }
    return outcome.GetResult().GetContentLength();
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
S3IODriver::S3IODriver (const char* bucket, const char* key, const Aws::Auth::AWSCredentials& credentials, Aws::Client::ClientConfiguration& config):
    client(credentials, nullptr, config)
{
    request.SetBucket(bucket);
    request.SetKey(key);
}
