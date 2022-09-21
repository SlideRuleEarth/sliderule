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
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

struct S3CurlIODriver::impl
{
    /*-----------------------------------------------
     * Data Typedef
     *-----------------------------------------------*/
    typedef struct {
        uint8_t*    buffer;
        int         size;
        int         index;
    } data_t;

    /*-----------------------------------------------
     * Constructor
     *-----------------------------------------------*/
    impl(CredentialStore::Credential* _credential, const char* _region):
        credential(_credential),
        region(_region)
    {
        /* Initialize cURL */
        curl = curl_easy_init();
        if(curl)
        {
            /* Set cURL Options */
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECTION_TIMEOUT); // seconds
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to initialize curl");
        }
    }

    /*-----------------------------------------------
     * Destructor
     *-----------------------------------------------*/
    ~impl()
    {
        curl_easy_cleanup(curl);
    }

    /*-----------------------------------------------
     * get
     *-----------------------------------------------*/
    void get (const char* bucket, const char* key, long offset, long length, uint8_t* buffer)
    {
        /* Build Host and URL String */
        SafeString host("%s.%s.s3.amazonaws.com", bucket, region);
        SafeString url("https://%s/%s", host.getString(), key);

        /* Build Date String */
        TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
        TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
        SafeString date("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);

        /* Build SecurityToken Header */
        SafeString securityTokenHeader("x-amz-security-token:%s", credential->sessionToken);

        /* Build StringToSign */
        SafeString stringToSign("GET\n\n\n%s\n%s\n/%s/%s", date.getString(), securityTokenHeader.getString(), bucket, key);

        /* Build Authorization Header */
        char hash[EVP_MAX_MD_SIZE];
        int hash_size = EVP_MAX_MD_SIZE;
        HMAC(EVP_sha1(), credential->secretAccessKey, strlen(credential->secretAccessKey), stringToSign.getString(), stringToSign.getLength() - 1, hash, &hash_size);
        SafeString authorizationHeader("Authorization: AWS %s:%s", credential->accessKeyId, stringToSign.getString());


        /* Setup Buffer for Callback */
        data_t data = {
            .buffer = buffer;
            .size = length,
            .index = 0
        };
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

        /* Set Options */
        curl_easy_setopt(curl, CURLOPT_URL, url.getString());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_READ_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, DEFAULT_SSL_VERIFYPEER);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, DEFAULT_SSL_VERIFYHOST);

        /* Build Request Headers */
        // TODO

        /* Perform Request */
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
        {
            /* Get HTTP Code */
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if(http_code != 200)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Http error <%ld> returned from S3 request", http_code);
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed: %ld", (long)res);
        }
    }

    /*-----------------------------------------------
     * curlWriteCallback
     *-----------------------------------------------*/
    static size_t curlWriteCallback(void *buffer, size_t size, size_t nmemb, void *userp)
    {
        data_t* data = (data_t*)userp;

        size_t rsps_size = size * nmemb;
        size_t bytes_available = data->size - data->index;
        size_t bytes_to_copy = MIN(rsps_size, bytes_available);

        LocalLib::copy(&data->buffer[data->index], buffer, bytes_to_copy);
        data->index += bytes_to_copy;

        return bytes_to_copy;
    }

    /*-----------------------------------------------
     * Data
     *-----------------------------------------------*/
    CredentialStore::Credential* credential;
    const char* region;
    CURL* curl;
};

/******************************************************************************
 * AWS S3 cURL I/O DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/
Mutex S3CurlIODriver::clientsMut;
Dictionary<S3CurlIODriver*> S3CurlIODriver::clients(STARTING_NUM_CLIENTS);
const char* S3CurlIODriver::FORMAT = "s3curl"

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3CurlIODriver::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void S3CurlIODriver::deinit (void)
{
    clientsMut.lock();
    {
        S3CurlIODriver* client;
        const char* key = clients.first(&client);
        while(key != NULL)
        {
            delete client->s3_client;
            delete [] client->asset_name;
            delete client;
            key = clients.next(&client);
        }
    }
    clientsMut.unlock();
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
IODriver* S3CurlIODriver::create (const Asset* _asset)
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
    CredentialStore::Credential latest_credential = CredentialStore::get(asset->getName());
    clientsMut.lock();
    {
        /* Try to Get Existing Client */
        if(clients.find(asset->getName(), &client))
        {
            client->reference_count++;
        }

        /* Check Need for New Client */
        if( (client == NULL) || // could not find an existing client
            (latest_credential.provided && (client->credential.expirationGps < latest_credential.expirationGps)) ) // existing client has outdated credentials
        {
            /* Destroy Old Client */
            if(client)
            {
                client->decommissioned = true;
                destroyClient(client);
            }

            /* Create Client */
            client = new client_t;
            client->credential = latest_credential;
            client->asset_name = StringLib::duplicate(asset->getName());
            client->reference_count = 1;
            client->decommissioned = false;
            client->s3_handle = new S3CurlIODriver::impl(&client->credential, asset->getRegion());

            /* Register New Client */
            clients.add(asset->getName(), client);
        }
    }
    clientsMut.unlock();
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
    asset(client);
    client->s3_handle->get(ioBucket, ioKey, pos, size, data);
    return size;
}

/*----------------------------------------------------------------------------
 * constructor
 *----------------------------------------------------------------------------*/
S3CurlIODriver::S3CurlIODriver (const Asset* _asset)
{
    asset = _asset;
    client = NULL;
    ioBucket = NULL;
    ioKey = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3CurlIODriver::~S3CurlIODriver (void)
{
    destroyClient();

    /*
     * Delete Memory Allocated for ioBucket
     *  only ioBucket is freed because ioKey only points
     *  into the memory allocated to ioBucket
     */
    if(ioBucket) delete [] ioBucket;
}

/*----------------------------------------------------------------------------
 * destroyClient
 *----------------------------------------------------------------------------*/
void S3CurlIODriver::destroyClient (void)
{
    assert(client);
    assert(client->reference_count > 0);

    clientsMut.lock();
    {
        client->reference_count--;
        if(client->decommissioned && (client->reference_count == 0))
        {
            clients.remove(client->asset_name);
            delete client->s3_client;
            delete [] client->asset_name;
            delete client->s3_handle;
            delete client;
        }
    }
    clientsMut.unlock();
}