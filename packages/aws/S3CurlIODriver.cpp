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
#include <openssl/sha.h>
#include <openssl/evp.h>


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
 * sha256hash
 *----------------------------------------------------------------------------*/
#define SHA256_HEX_STR_SIZE ((SHA256_DIGEST_LENGTH * 2) + 1)
static void sha256hash(const void* data, size_t len, char* dst)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hash_size = 0;
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if(EVP_DigestInit_ex(context, EVP_sha256(), NULL))
    if(EVP_DigestUpdate(context, data, len))
    if(EVP_DigestFinal_ex(context, hash, &hash_size))
    assert(hash_size == SHA256_DIGEST_LENGTH);
    EVP_MD_CTX_free(context);
    StringLib::b16encode(hash, SHA256_DIGEST_LENGTH, true, dst);
}

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
    List<streaming_data_t>* rsps_set = (List<streaming_data_t>*)userp;
    streaming_data_t rsps;
    rsps.size = size * nmemb;
    rsps.data = new char [rsps.size];
    LocalLib::copy(rsps.data, buffer, rsps.size);
    rsps_set->add(rsps);
    return rsps.size;
}

/*----------------------------------------------------------------------------
 * curlWriteFile
 *----------------------------------------------------------------------------*/
static size_t curlWriteFile(void *buffer, size_t size, size_t nmemb, void *userp)
{
    file_data_t* data = (file_data_t*)userp;
    size_t rsps_size = size * nmemb;
    size_t bytes_written = fwrite(buffer, 1, rsps_size, data->fd);
    if(bytes_written > 0) data->size += rsps_size;
    return bytes_written;
}

/*----------------------------------------------------------------------------
 * curlReadFile
 *----------------------------------------------------------------------------*/
static size_t curlReadFile(void* buffer, size_t size, size_t nmemb, void *userp)
{
    file_data_t* data = (file_data_t*)userp;

    size_t buffer_size = size * nmemb;
    size_t bytes_read = fread(buffer, 1, buffer_size, data->fd);
    if(bytes_read) data->size += bytes_read;

    return bytes_read;
}

/*----------------------------------------------------------------------------
 * buildReadHeadersV2
 *----------------------------------------------------------------------------*/
static headers_t buildReadHeadersV2 (const char* bucket, const char* key, CredentialStore::Credential* credentials)
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
        SafeString securityTokenHeader("x-amz-security-token:%s", credentials->sessionToken);
        headers = curl_slist_append(headers, securityTokenHeader.getString());

        /* Build Authorization Header */
        SafeString stringToSign("GET\n\n\n%s\n%s\n/%s/%s", date.getString(), securityTokenHeader.getString(), bucket, key);
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
 * buildWriteHeadersV2
 *----------------------------------------------------------------------------*/
static headers_t buildWriteHeadersV2 (const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials, long content_length)
{
    (void)region;

    /* Initial HTTP Header List */
    struct curl_slist* headers = NULL;

    /* Build Date String and Date Header */
    TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
    TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
    SafeString date("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);
    SafeString dateHeader("Date: %s", date.getString());
    headers = curl_slist_append(headers, dateHeader.getString());

    /* Content Headers */
    SafeString contentType("application/octet-stream");
    SafeString contentTypeHeader("Content-Type: %s", contentType.getString());
    headers = curl_slist_append(headers, contentTypeHeader.getString());
    SafeString contentLengthHeader("Content-Length: %ld", content_length);
    headers = curl_slist_append(headers, contentLengthHeader.getString());

    /* Initialize and Remove Unwanted Headers */
    headers = curl_slist_append(headers, "Transfer-Encoding:");

    if(credentials && credentials->provided)
    {
        /* Build SecurityToken Header */
        SafeString securityTokenHeader("x-amz-security-token:%s", credentials->sessionToken);
        headers = curl_slist_append(headers, securityTokenHeader.getString());

        /* Build Authorization Header */
        SafeString stringToSign("PUT\n\n%s\n%s\n%s\n/%s/%s", contentType.getString(), date.getString(), securityTokenHeader.getString(), bucket, key);
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
 * buildWriteHeadersV4
 *
 * Does not work, the response from AWS is:
 *  "The AWS Access Key Id you provided does not exist in our records."
 *
 * Looks like the issue is related to how to provide the session token
 * when using temporary credentials
 *----------------------------------------------------------------------------*/
#if 0
static headers_t buildWriteHeadersV4 (const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials, long content_length)
{
    /* Must Supply Credentials */
    if(!credentials || !credentials->provided)
    {
        return NULL;
    }

    /* Build Date String */
    TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
    TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
    SafeString timestamp("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);

    /* Build Canonical Request */
    char canonical_request_hash[SHA256_HEX_STR_SIZE];
    SafeString canonical_request("PUT\n/%s\n\ncontent-length:%ld\ndate:%s\nhost:%s.s3.amazonaws.com\nx-amz-content-sha256:UNSIGNED-PAYLOAD\nx-amz-date:%s\nx-amz-security-token:%s\n\ncontent-length;date;host;x-amz-content-sha256;x-amz-date;x-amz-security-token\nUNSIGNED-PAYLOAD",
                                    key, content_length, timestamp.getString(), bucket, timestamp.getString(), credentials->sessionToken);
    sha256hash(canonical_request.getString(), canonical_request.getLength() - 1, canonical_request_hash);

    /* Build String To Sign */
    SafeString date("%04d%02d%02d", gmt_date.year, gmt_date.month, gmt_date.day);
    SafeString scope("%s/%s/s3/aws4_request", date.getString(), region);
    SafeString str2sign("AWS4-HMAC-SHA256\n%s\n%s\n%s", timestamp.getString(), scope.getString(), canonical_request_hash);

    /* Calculate Signature */
    SafeString secret_access_key_str2sign("AWS4%s", credentials->secretAccessKey);

    unsigned char date_key[EVP_MAX_MD_SIZE];
    unsigned int date_key_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha256(), secret_access_key_str2sign.getString(), secret_access_key_str2sign.getLength() - 1, (unsigned char*)date.getString(), date.getLength() - 1, date_key, &date_key_size);

    unsigned char date_region_key[EVP_MAX_MD_SIZE];
    unsigned int date_region_key_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha256(), date_key, date_key_size, (unsigned char*)region, StringLib::size(region), date_region_key, &date_region_key_size);

    unsigned char date_region_service_key[EVP_MAX_MD_SIZE];
    unsigned int date_region_service_key_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha256(), date_region_key, date_region_key_size, (unsigned char*)"s3", 2, date_region_service_key, &date_region_service_key_size);

    unsigned char signing_key[EVP_MAX_MD_SIZE];
    unsigned int signing_key_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha256(), date_region_service_key, date_region_service_key_size, (unsigned char*)"aws4_request", 12, signing_key, &signing_key_size);

    unsigned char signature[EVP_MAX_MD_SIZE];
    unsigned int signature_size = EVP_MAX_MD_SIZE; // set below with actual size
    HMAC(EVP_sha256(), signing_key, signing_key_size, (unsigned char*)str2sign.getString(), str2sign.getLength() - 1, signature, &signature_size);

    char signature_hex[SHA256_HEX_STR_SIZE];
    StringLib::b16encode(signature, signature_size, true, signature_hex);

    /* Initialize and Remove Unwanted Headers */
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Transfer-Encoding:");

    /* Build Headers */
    SafeString date_hdr("Date: %s", timestamp.getString());
    headers = curl_slist_append(headers, date_hdr.getString());
    SafeString content_length_hdr("Content-Length: %ld", content_length);
    headers = curl_slist_append(headers, content_length_hdr.getString());
    SafeString auth_hdr("Authorization: AWS4-HMAC-SHA256 Credential=%s/%s/%s/s3/aws4_request,SignedHeaders=content-length;date;host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature=%s", credentials->accessKeyId, date.getString(), region, signature_hex);
    headers = curl_slist_append(headers, auth_hdr.getString());
    SafeString amz_date_hdr("x-amz-date: %s", timestamp.getString());
    headers = curl_slist_append(headers, amz_date_hdr.getString());
    SafeString amz_token_hdr("x-amz-security-token: %s", credentials->sessionToken);
    headers = curl_slist_append(headers, amz_date_hdr.getString());
    SafeString amz_content_sha256_hdr("x-amz-content-sha256: %s", "UNSIGNED-PAYLOAD");
    headers = curl_slist_append(headers, amz_content_sha256_hdr.getString());

    /* Return Headers */
    return headers;
}
#endif
/*----------------------------------------------------------------------------
 * initializeReadRequest
 *----------------------------------------------------------------------------*/
static CURL* initializeReadRequest (SafeString& url, headers_t headers, write_cb_t write_cb, void* write_parm)
{
    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    if(curl)
    {
        /* Set Options */
        curl_easy_setopt(curl, CURLOPT_URL, url.getString());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, S3CurlIODriver::READ_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, S3CurlIODriver::CONNECTION_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, S3CurlIODriver::LOW_SPEED_TIME);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, S3CurlIODriver::LOW_SPEED_LIMIT);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, S3CurlIODriver::SSL_VERIFYPEER);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, S3CurlIODriver::SSL_VERIFYHOST);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_parm);
    }
    else
    {
        mlog(CRITICAL, "Failed to initialize cURL request");
    }

    /* Return Handle */
    return curl;
}

/*----------------------------------------------------------------------------
 * initializeWriteRequest
 *----------------------------------------------------------------------------*/
static CURL* initializeWriteRequest (SafeString& url, headers_t headers, write_cb_t read_cb, void* read_parm)
{
    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    if(curl)
    {
        /* Set Options */
        curl_easy_setopt(curl, CURLOPT_URL, url.getString());
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, S3CurlIODriver::READ_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, S3CurlIODriver::CONNECTION_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, S3CurlIODriver::LOW_SPEED_TIME);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, S3CurlIODriver::LOW_SPEED_LIMIT);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, S3CurlIODriver::SSL_VERIFYPEER);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, S3CurlIODriver::SSL_VERIFYHOST);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, read_parm);

    }
    else
    {
        mlog(CRITICAL, "Failed to initialize cURL put request");
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
 * get - fixed
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::get (uint8_t* data, int64_t size, uint64_t pos, const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials)
{
    bool status = false;

    /* Massage Key */
    const char* key_ptr = key;
    if(key_ptr[0] == '/') key_ptr++;

    /* Build URL */
    SafeString url("https://s3.%s.amazonaws.com/%s/%s", region, bucket, key_ptr);

    /* Setup Buffer for Callback */
    fixed_data_t info = {
        .buffer = data,
        .size = size,
        .index = 0
    };

    /* Issue Get Request */
    int attempts = ATTEMPTS_PER_REQUEST;
    bool rqst_complete = false;
    while(!rqst_complete && (attempts > 0))
    {
        /* Build Standard Headers */
        struct curl_slist* headers = buildReadHeadersV2(bucket, key_ptr, credentials);

        /* Build Range Header */
        unsigned long start_byte = pos + info.index;
        unsigned long end_byte = pos + size - info.index - 1;
        SafeString rangeHeader("Range: bytes=%lu-%lu", start_byte, end_byte);
        headers = curl_slist_append(headers, rangeHeader.getString());

        /* Initialize cURL Request */
        CURL* curl = initializeReadRequest(url, headers, curlWriteFixed, &info);
        if(curl)
        {
            while(!rqst_complete && (attempts-- > 0))
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

                    /* Get Request Completed */
                    rqst_complete = true;
                }
                else if(info.index > 0)
                {
                    mlog(CRITICAL, "cURL error (%d) encountered after partial response (%ld): %s", res, info.index, key_ptr);
                    rqst_complete = true;
                }
                else if(res == CURLE_OPERATION_TIMEDOUT)
                {
                    mlog(CRITICAL, "cURL call timed out (%d) for request: %s", res, key_ptr);
                }
                else
                {
                    mlog(CRITICAL, "cURL call failed (%d) for request: %s", res, key_ptr);
                    LocalLib::performIOTimeout();
                }
            }

            /* Clean Up cURL */
            curl_easy_cleanup(curl);
        }
        else
        {
            /* Decrement Attempts on Failed cURL Initialization */
            attempts--;
        }

        /* Clean Up Headers */
        curl_slist_free_all(headers);
    }

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL fixed request to S3 failed");
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
    struct curl_slist* headers = buildReadHeadersV2(bucket, key_ptr, credentials);

    /* Setup Streaming Data for Callback */
    List<streaming_data_t> rsps_set;

    /* Build URL */
    SafeString url("https://s3.%s.amazonaws.com/%s/%s", region, bucket, key_ptr);

    /* Initialize cURL Request */
    bool rqst_complete = false;
    int attempts = ATTEMPTS_PER_REQUEST;
    CURL* curl = initializeReadRequest(url, headers, curlWriteStreaming, &rsps_set);
    if(curl)
    {
        while(!rqst_complete && (attempts-- > 0))
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

                /* Request Completed */
                rqst_complete = true;
            }
            else if(rsps_set.length() > 0)
            {
                mlog(CRITICAL, "cURL error (%d) encountered after partial response (%d): %s", res, rsps_set.length(), key_ptr);
                rqst_complete = true;
            }
            else if(res == CURLE_OPERATION_TIMEDOUT)
            {
                mlog(CRITICAL, "cURL call timed out (%d) for request: %s", res, key_ptr);
            }
            else
            {
                mlog(CRITICAL, "cURL call failed (%d) for request: %s", res, key_ptr);
                LocalLib::performIOTimeout();
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
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL streaming request to S3 failed");
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
    struct curl_slist* headers = buildReadHeadersV2(bucket, key_ptr, credentials);

    /* Setup File Data for Callback */
    file_data_t data;
    data.size = 0;
    data.fd = fopen(filename, "w");
    if(data.fd)
    {
        /* Build URL */
        SafeString url("https://s3.%s.amazonaws.com/%s/%s", region, bucket, key_ptr);

        /* Initialize cURL Request */
        bool rqst_complete = false;
        int attempts = ATTEMPTS_PER_REQUEST;
        CURL* curl = initializeReadRequest(url, headers, curlWriteFile, &data);
        if(curl)
        {
            while(!rqst_complete && (attempts-- > 0))
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

                    /* Request Completed */
                    rqst_complete = true;
                }
                else if(data.size > 0)
                {
                    mlog(CRITICAL, "cURL error (%d) encountered after partial response (%ld): %s", res, data.size, key_ptr);
                    rqst_complete = true;
                }
                else if(res == CURLE_OPERATION_TIMEDOUT)
                {
                    mlog(CRITICAL, "cURL call timed out (%d) for request: %s", res, key_ptr);
                }
                else
                {
                    mlog(CRITICAL, "cURL call failed (%d) for request: %s", res, key_ptr);
                    LocalLib::performIOTimeout();
                }
            }

            /* Clean Up cURL */
            curl_easy_cleanup(curl);
        }

        /* Close File */
        fclose(data.fd);
    }
    else
    {
        mlog(CRITICAL, "Failed to open destination file %s for writing: %s", filename, LocalLib::err2str(errno));
    }

    /* Clean Up Headers */
    curl_slist_free_all(headers);

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL file request to S3 failed");
    }

    /* Return Success */
    return data.size;
}

/*----------------------------------------------------------------------------
 * put - file
 *----------------------------------------------------------------------------*/
int64_t S3CurlIODriver::put (const char* filename, const char* bucket, const char* key, const char* region, CredentialStore::Credential* credentials)
{
    bool status = false;

    /* Massage Key */
    const char* key_ptr = key;
    if(key_ptr[0] == '/') key_ptr++;

    /* Setup File Data for Callback */
    file_data_t data;
    data.size = 0;
    data.fd = fopen(filename, "r");
    if(data.fd)
    {
        /* Get Size of File */
        fseek(data.fd, 0L, SEEK_END);
        long content_length = ftell(data.fd);
        fseek(data.fd, 0L, SEEK_SET);

        /* Build Headers */
        struct curl_slist* headers = buildWriteHeadersV2(bucket, key_ptr, region, credentials, content_length);

        /* Build URL */
        SafeString url("https://s3.%s.amazonaws.com/%s/%s", region, bucket, key_ptr);

        /* Initialize cURL Request */
        bool rqst_complete = false;
        int attempts = ATTEMPTS_PER_REQUEST;
        CURL* curl = initializeWriteRequest(url, headers, curlReadFile, &data);
        if(curl)
        {
            while(!rqst_complete && (attempts-- > 0))
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

                    /* Request Completed */
                    rqst_complete = true;
                }
                else if(data.size > 0)
                {
                    mlog(CRITICAL, "cURL error (%d) encountered after partial response (%ld): %s", res, data.size, key_ptr);
                    rqst_complete = true;
                }
                else if(res == CURLE_OPERATION_TIMEDOUT)
                {
                    mlog(CRITICAL, "cURL call timed out (%d) for request: %s", res, key_ptr);
                }
                else
                {
                    mlog(CRITICAL, "cURL call failed (%d) for put request: %s", res, key_ptr);
                    LocalLib::performIOTimeout();
                }
            }

            /* Clean Up cURL */
            curl_easy_cleanup(curl);
        }

        /* Clean Up Headers */
        curl_slist_free_all(headers);

        /* Close File */
        fclose(data.fd);
    }
    else
    {
        mlog(CRITICAL, "Failed to open source file %s for reading: %s", filename, LocalLib::err2str(errno));
    }

    /* Throw Exception on Failure */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL file request to S3 failed");
    }

    /* Return Success */
    return data.size;
}

/*----------------------------------------------------------------------------
 * luaGet - s3get(<bucket>, <key>, [<region>], [<asset>]) -> contents
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
        if(rsps_data)
        {
            lua_pushlstring(L, (char*)rsps_data, rsps_size);
            delete [] rsps_data;
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
 * luaDownload - s3download(<bucket>, <key>, [<region>], [<asset>]) -> file
 *----------------------------------------------------------------------------*/
int S3CurlIODriver::luaDownload(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        const char* region      = LuaObject::getLuaString(L, 3, true, S3CurlIODriver::DEFAULT_REGION);
        const char* asset_name  = LuaObject::getLuaString(L, 4, true, S3CurlIODriver::DEFAULT_ASSET_NAME);
        const char* filename    = LuaObject::getLuaString(L, 5, true, key);

        /* Get Credentials */
        CredentialStore::Credential credentials = CredentialStore::get(asset_name);

        /* Make Request */
        int64_t rsps_size = get(filename, bucket, key, region, &credentials);

        /* Push Contents */
        if(rsps_size > 0)   status = true;
        else                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read %s/%s", bucket, key);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting S3 object: %s", e.what());
    }

    /* Return Results */
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaRead - s3read(<bucket>, <key>, <size>, <pos>, [<region>], [<asset>]) -> contents
 *----------------------------------------------------------------------------*/
int S3CurlIODriver::luaRead(lua_State* L)
{
    bool status = false;
    int num_rets = 1;

    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        long size               = LuaObject::getLuaInteger(L, 3);
        long pos                = LuaObject::getLuaInteger(L, 4);
        const char* region      = LuaObject::getLuaString(L, 5, true, S3CurlIODriver::DEFAULT_REGION);
        const char* asset_name  = LuaObject::getLuaString(L, 6, true, S3CurlIODriver::DEFAULT_ASSET_NAME);

        /* Check Parameters */
        if(size <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid size: %ld", size);
        else if(pos < 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid position: %ld", pos);

        /* Get Credentials */
        CredentialStore::Credential credentials = CredentialStore::get(asset_name);

        /* Make Request */
        uint8_t* rsps_data = new uint8_t [size];
        int64_t rsps_size = get(rsps_data, size, pos, bucket, key, region, &credentials);

        /* Push Contents */
        if(rsps_size > 0)
        {
            lua_pushlstring(L, (char*)rsps_data, rsps_size);
            delete [] rsps_data;
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
 * luaUpload - s3upload(<bucket>, <key>, <filename>, [<region>], [<asset>])
 *----------------------------------------------------------------------------*/
int S3CurlIODriver::luaUpload(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);
        const char* filename    = LuaObject::getLuaString(L, 3);
        const char* region      = LuaObject::getLuaString(L, 4, true, S3CurlIODriver::DEFAULT_REGION);
        const char* asset_name  = LuaObject::getLuaString(L, 5, true, S3CurlIODriver::DEFAULT_ASSET_NAME);

        /* Get Credentials */
        CredentialStore::Credential credentials = CredentialStore::get(asset_name);

        /* Make Request */
        int64_t upload_size = put(filename, bucket, key, region, &credentials);

        /* Push Contents */
        if(upload_size > 0)
        {
            lua_pushnumber(L, upload_size);
            status = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to upload %s/%s", bucket, key);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error uploading S3 object: %s", e.what());
    }

    /* Return Results */
    lua_pushboolean(L, status);
    return 1;
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
