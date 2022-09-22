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

#include "S3CurlIODriver.h"
#include "CredentialStore.h"
#include "core.h"

#include <curl/curl.h>
#include <openssl/hmac.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3CurlIODriver::FORMAT = "s3curl";

/******************************************************************************
 * AWS S3 cURL I/O DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* S3CurlIODriver::create (const Asset* _asset)
{
    return new S3CurlIODriver(_asset);
}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
void S3CurlIODriver::ioOpen (const char* resource)
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
    credentials = CredentialStore::get(asset->getName());
}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
void S3CurlIODriver::ioClose (void)
{

}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    //TODO: Handle when credentials are not provided

    /* Build Host and URL String */
    SafeString host("%s.%s.s3.amazonaws.com", ioBucket, asset->getRegion());
    SafeString url("https://%s/%s", host.getString(), ioKey);

    /* Build Date String and Date Header */
    TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
    TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
    SafeString date("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);
    SafeString dateHeader("Date: %s", date.getString());

    /* Build Range Header */
    SafeString rangeHeader("Range: bytes=%lu-%lu", (unsigned long)pos, (unsigned long)(pos + size - 1));

    /* Build SecurityToken Header */
    SafeString securityTokenHeader("x-amz-security-token:%s", credentials.sessionToken);

    /* Build StringToSign */
    SafeString stringToSign("GET\n\n\n%s\n%s\n/%s/%s", date.getString(), securityTokenHeader.getString(), ioBucket, ioKey);

    /* Build Authorization Header */
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha1(), credentials.secretAccessKey, StringLib::size(credentials.secretAccessKey), (unsigned char*)stringToSign.getString(), stringToSign.getLength() - 1, hash, &hash_size);
    SafeString encodedHash(64, hash, hash_size);
    encodedHash.urlize();
    SafeString authorizationHeader("Authorization: AWS %s:%s", credentials.accessKeyId, encodedHash.getString());

    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    try
    {
        if(!curl)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to initialize curl");
        }

        /* Set Request Headers */
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, dateHeader.getString());
        headers = curl_slist_append(headers, rangeHeader.getString());
        headers = curl_slist_append(headers, authorizationHeader.getString());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* Setup Buffer for Callback */
        data_t info = {
            .buffer = data,
            .size = size,
            .index = 0
        };

        /* Set Options */
        curl_easy_setopt(curl, CURLOPT_URL, url.getString());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_READ_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECTION_TIMEOUT); // seconds
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, DEFAULT_SSL_VERIFYPEER);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, DEFAULT_SSL_VERIFYHOST);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &info);

        /* Perform Request */
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
        {
            /* Get HTTP Code */
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if(http_code != 200)
            {
                StringLib::printify((char*)info.buffer, info.index);
                mlog(INFO, "%s", info.buffer);
                throw RunTimeException(CRITICAL, RTE_ERROR, "S3 get returned http error <%ld>", http_code);
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed: %ld", (long)res);
        }
    }
    catch(const RunTimeException& e)
    {
        curl_easy_cleanup(curl);
        throw; // rethrow after cleaning up curl
    }

    /* Return Success */
    curl_easy_cleanup(curl);
    return size;
}

/*----------------------------------------------------------------------------
 * constructor
 *----------------------------------------------------------------------------*/
S3CurlIODriver::S3CurlIODriver (const Asset* _asset)
{
    asset = _asset;
    ioBucket = NULL;
    ioKey = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3CurlIODriver::~S3CurlIODriver (void)
{
    /*
     * Delete Memory Allocated for ioBucket
     *  only ioBucket is freed because ioKey only points
     *  into the memory allocated to ioBucket
     */
    if(ioBucket) delete [] ioBucket;
}

/*----------------------------------------------------------------------------
 * curlWriteCallback
 *----------------------------------------------------------------------------*/
size_t S3CurlIODriver::curlWriteCallback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    data_t* data = (data_t*)userp;

    size_t rsps_size = size * nmemb;
    size_t bytes_available = data->size - data->index;
    size_t bytes_to_copy = MIN(rsps_size, bytes_available);

    LocalLib::copy(&data->buffer[data->index], buffer, bytes_to_copy);
    data->index += bytes_to_copy;

    return bytes_to_copy;
}
