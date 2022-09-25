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
 * LOCAL TYPEDEFS
 ******************************************************************************/

typedef struct {
    uint8_t*    buffer;
    long        size;
    long        index;
} fixed_data_t;

typedef struct {
    char*       data;
    long        size;
} streaming_data_t;

typedef struct {
    FILE*       fd;
    long        size;
} file_data_t;

typedef struct curl_slist* headers_t;

typedef size_t (*write_cb_t)(void*, size_t, size_t, void*);

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * curlWriteFixed
 *----------------------------------------------------------------------------*/
static size_t curlWriteFixed(void *buffer, size_t size, size_t nmemb, void *userp)
{
    fixed_data_t* data = (fixed_data_t*)userp;
    size_t rsps_size = size * nmemb;
    size_t bytes_available = data->size - data->index;
    size_t bytes_to_copy = MIN(rsps_size, bytes_available);
    LocalLib::copy(&data->buffer[data->index], buffer, bytes_to_copy);
    data->index += bytes_to_copy;
    return bytes_to_copy;
}

/*----------------------------------------------------------------------------
 * curlWriteStreaming
 *----------------------------------------------------------------------------*/
static size_t curlWriteStreaming(void *buffer, size_t size, size_t nmemb, void *userp)
{
    file_data_t* data = (file_data_t*)userp;
    size_t rsps_size = size * nmemb;
    size_t bytes_written = fwrite(buffer, rsps_size, 1, data->fd);
    if(bytes_written > 0) data->size += rsps_size;
    return bytes_written;
}

/*----------------------------------------------------------------------------
 * curlWriteFile
 *----------------------------------------------------------------------------*/
static size_t curlWriteFile(void *buffer, size_t size, size_t nmemb, void *userp)
{
    List<streaming_data_t>* rsps_set = (List<streaming_data_t>*)userp;
    streaming_data_t rsps;
    rsps.size = size * nmemb;
    rsps.data = new char [rsps.size];
    LocalLib::copy(rsps.data, buffer, rsps.size);
    rsps_set->add(rsps);
    return rsps.size;
}

/*----------------------------------------------------------------------------
 * buildHeaders
 *----------------------------------------------------------------------------*/
static headers_t buildHeaders (const char* bucket, const char* key, CredentialStore::Credential* credentials)
{
    /* Initial HTTP Header List */
    struct curl_slist* headers = NULL;

    /* Build Date String and Date Header */
    TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
    TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
    SafeString date("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);
    SafeString dateHeader("Date: %s", date.getString());
    headers = curl_slist_append(headers, dateHeader.getString());

    if(credentials && credentials->provided)
    {
        /* Build SecurityToken Header */
        SafeString securityTokenHeaderToSign("x-amz-security-token:%s", credentials->sessionToken);
        SafeString securityTokenHeader("X-AMZ-Security-Token: %s", credentials->sessionToken);
        headers = curl_slist_append(headers, securityTokenHeader.getString());

        /* Build Authorization Header */
        SafeString stringToSign("GET\n\n\n%s\n%s\n/%s/%s", date.getString(), securityTokenHeaderToSign.getString(), bucket, key);
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_size = EVP_MAX_MD_SIZE; // set below with actual size
        HMAC(EVP_sha1(), credentials->secretAccessKey, StringLib::size(credentials->secretAccessKey), (unsigned char*)stringToSign.getString(), stringToSign.getLength() - 1, hash, &hash_size);
        SafeString encodedHash(64, hash, hash_size);
        SafeString authorizationHeader("Authorization: AWS %s:%s", credentials->accessKeyId, encodedHash.getString());
        headers = curl_slist_append(headers, authorizationHeader.getString());
    }

    /* Return */
    return headers;
}

/*----------------------------------------------------------------------------
 * initializeRequest
 *----------------------------------------------------------------------------*/
static CURL* initializeRequest (const char* bucket, const char* key, const char* region, headers_t headers, write_cb_t write_cb, void* write_parm)
{
    /* Build URL String */
    SafeString url("https://s3.%s.amazonaws.com/%s/%s", region, bucket, key);

    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    if(curl)
    {
        /* Set Options */
        curl_easy_setopt(curl, CURLOPT_URL, url.getString());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, S3CurlIODriver::DEFAULT_READ_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, S3CurlIODriver::DEFAULT_CONNECTION_TIMEOUT); // seconds
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, S3CurlIODriver::DEFAULT_SSL_VERIFYPEER);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, S3CurlIODriver::DEFAULT_SSL_VERIFYHOST);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_parm);
    }
    else
    {
        mlog(CRITICAL, "cURL failed to initialize");
    }

    /* Return Handle */
    return curl;
}

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3CurlIODriver::DEFAULT_REGION = "us-west-2";
const char* S3CurlIODriver::DEFAULT_ASSET_NAME = "iam-role";
const char* S3CurlIODriver::FORMAT = "s3";

/******************************************************************************
 * AWS S3 cURL I/O DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* S3CurlIODriver::create (const Asset* _asset, const char* resource)
{
    return new S3CurlIODriver(_asset, resource);
}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    return get(data, size, pos, ioBucket, ioKey, asset->getRegion(), &latestCredentials);
}

/*----------------------------------------------------------------------------
 * luaGet - s3get(<bucket>, <key>, [<region>], [<endpoint>]) -> contents
 *----------------------------------------------------------------------------*/
int S3CurlIODriver::luaGet(lua_State* L)
{
    bool status = false;
    int num_rets = 1;

    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        const char* region      = LuaObject::getLuaString(L, 3, true, S3CurlIODriver::DEFAULT_REGION);
        const char* asset_name  = LuaObject::getLuaString(L, 4, true, S3CurlIODriver::DEFAULT_ASSET_NAME);

        /* Get Credentials */
        CredentialStore::Credential credentials = CredentialStore::get(asset_name);

        /* Make Request */
        uint8_t* rsps_data = NULL;
        int64_t rsps_size = get(&rsps_data, bucket, key, region, &credentials);

        /* Push Contents */
        if(*rsps_data)
        {
            lua_pushlstring(L, (char*)rsps_data, rsps_size);
            status = true;
            num_rets++;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read %s/%s", bucket, key);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting S3 object: %s", e.what());
    }

    /* Return Results */
    lua_pushboolean(L, status);
    return num_rets;
}

/*----------------------------------------------------------------------------
 * Constructor - for derived classes
 *----------------------------------------------------------------------------*/
S3CurlIODriver::S3CurlIODriver (const Asset* _asset):
    asset(_asset)
{
    ioBucket = NULL;
    ioKey = NULL;

    /* Get Latest Credentials */
    latestCredentials = CredentialStore::get(asset->getName());
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
S3CurlIODriver::S3CurlIODriver (const Asset* _asset, const char* resource):
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
    latestCredentials = CredentialStore::get(asset->getName());
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
 * get - fixed
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::get (uint8_t* data, int64_t size, uint64_t pos, const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials)
{
    bool status = false;

    /* Massage Key */
    const char* key_ptr = key;
    if(key_ptr[0] == '/') key_ptr++;

    /* Build Headers */
    struct curl_slist* headers = buildHeaders(bucket, key_ptr, credentials);
    SafeString rangeHeader("Range: bytes=%lu-%lu", (unsigned long)pos, (unsigned long)(pos + size - 1));
    headers = curl_slist_append(headers, rangeHeader.getString());

    /* Setup Buffer for Callback */
    fixed_data_t info = {
        .buffer = data,
        .size = size,
        .index = 0
    };

    /* Initialize cURL Request */
    CURL* curl = initializeRequest(bucket, key_ptr, region, headers, curlWriteFixed, &info);
    if(curl)
    {
        /* Perform Request */
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
        {
            /* Get HTTP Code */
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if(http_code < 300)
            {
                /* Request Succeeded */
                status = true;
            }
            else
            {
                /* Request Failed */
                StringLib::printify((char*)info.buffer, info.index);
                mlog(INFO, "%s", info.buffer);
                mlog(CRITICAL, "S3 get returned http error <%ld>", http_code);
            }
        }

        /* Clean Up cURL */
        curl_easy_cleanup(curl);
    }

    /* Clean Up Headers */
    curl_slist_free_all(headers);

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed");
    }

    /* Return Success */
    return size;
}

/*----------------------------------------------------------------------------
 * get - streaming
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::get (uint8_t** data, const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials)
{
    /* Initialize Function Parameters */
    bool status = false;
    int64_t rsps_size = 0;
    *data = NULL;

    /* Massage Key */
    const char* key_ptr = key;
    if(key_ptr[0] == '/') key_ptr++;

    /* Build Headers */
    struct curl_slist* headers = buildHeaders(bucket, key_ptr, credentials);

    /* Setup Streaming Data for Callback */
    List<streaming_data_t> rsps_set;

    /* Initialize cURL Request */
    CURL* curl = initializeRequest(bucket, key_ptr, region, headers, curlWriteStreaming, &rsps_set);
    if(curl)
    {
        /* Perform Request */
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
        {
            /* Get Response Size */
            for(int i = 0; i < rsps_set.length(); i++)
            {
                rsps_size += rsps_set[i].size;
            }

            /* Allocate and Populate Response */
            int rsps_index = 0;
            *data = new uint8_t [rsps_size + 1];
            uint8_t* rsps = *data; // reads easier below
            for(int i = 0; i < rsps_set.length(); i++)
            {
                LocalLib::copy(&rsps[rsps_index], rsps_set[i].data, rsps_set[i].size);
                rsps_index += rsps_set[i].size;
            }
            rsps[rsps_index] = '\0';

            /* Get HTTP Code */
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if(http_code < 300)
            {
                /* Request Succeeded */
                status = true;
            }
            else
            {
                /* Request Failed */
                StringLib::printify((char*)rsps, rsps_size + 1);
                mlog(INFO, "%s", (const char*)rsps);
                delete [] *data; // clean up memory
                *data = NULL;
                mlog(CRITICAL, "S3 get returned http error <%ld>", http_code);
            }
        }

        /* Clean Up cURL */
        curl_easy_cleanup(curl);
    }

    /* Clean Up Headers */
    curl_slist_free_all(headers);

    /* Clean Up Response List */
    for(int i = 0; i < rsps_set.length(); i++)
    {
        delete [] rsps_set[i].data;
    }

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed");
    }

    /* Return Success */
    return rsps_size;
}

/*----------------------------------------------------------------------------
 * get - file
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::get (const char* filename, const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials)
{
    bool status = false;

    /* Massage Key */
    const char* key_ptr = key;
    if(key_ptr[0] == '/') key_ptr++;

    /* Build Headers */
    struct curl_slist* headers = buildHeaders(bucket, key_ptr, credentials);

    /* Setup File Data for Callback */
    file_data_t data;
    data.size = 0;
    data.fd = fopen(filename, "w");
    if(data.fd)
    {
        /* Initialize cURL Request */
        CURL* curl = initializeRequest(bucket, key_ptr, region, headers, curlWriteFile, &data);
        if(curl)
        {
            /* Perform Request */
            CURLcode res = curl_easy_perform(curl);
            if(res == CURLE_OK)
            {
                /* Get HTTP Code */
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                if(http_code < 300)
                {
                    /* Request Succeeded */
                    status = true;
                }
                else
                {
                    /* Request Failed */
                    mlog(CRITICAL, "S3 get returned http error <%ld>", http_code);
                }
            }

            /* Clean Up cURL */
            curl_easy_cleanup(curl);
        }

        /* Close File */
        fclose(data.fd);
    }

    /* Clean Up Headers */
    curl_slist_free_all(headers);

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed");
    }

    /* Return Success */
    return data.size;
}
