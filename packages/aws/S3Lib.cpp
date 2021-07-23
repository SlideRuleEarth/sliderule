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


#include "S3Lib.h"
#include "CredentialStore.h"
#include "core.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentials.h>

/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/
/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/

Mutex S3Lib::clientsMut;
Dictionary<S3Lib::client_t> S3Lib::clients(STARTING_NUM_CLIENTS);

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3Lib::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void S3Lib::deinit (void)
{
    clientsMut.lock();
    {
        client_t client;
        const char* key = clients.first(&client);
        while(key != NULL)
        {
            if(client.s3_client) delete client.s3_client;
            key = clients.next(&client);
        }
    }
    clientsMut.unlock();
}

/*----------------------------------------------------------------------------
 * createClient
 *----------------------------------------------------------------------------*/
Aws::S3::S3Client* S3Lib::createClient (const Asset* asset)
{
    client_t client = { .s3_client = NULL, .expiration_gps = 0L };
    bool new_s3_client = false;

    /* Try to Get Existing Client */
    clientsMut.lock();
    {
        clients.find(asset->getName(), &client);
    }
    clientsMut.unlock();

    /* Look Up Credentials */
    CredentialStore::Credential credential = CredentialStore::get(asset->getPath());
    if(credential.expiration)
    {
        /* Calculate Time to Live */
        int64_t now = TimeLib::gettimems();
        double time_to_live = (double)(credential.expirationGps - now) / 1000.0; // seconds
        if(time_to_live > 0.0)
        {
            /* Check for New Credentials*/
            if(credential.expirationGps > client.expiration_gps)
            {
                /* Build Credentials */
                const Aws::String accessKeyId(credential.accessKeyId);
                const Aws::String secretAccessKey(credential.secretAccessKey);
                const Aws::String sessionToken(credential.sessionToken);
                Aws::Auth::AWSCredentials credentials(accessKeyId, secretAccessKey, sessionToken);

                /* Create S3 Client Configuration */
                Aws::Client::ClientConfiguration client_config;
                client_config.endpointOverride = asset->getEndpoint();
                client_config.region = asset->getRegion();

                /* Create S3 Client */
                if(client.s3_client) delete client.s3_client;
                client.s3_client = new Aws::S3::S3Client(credentials, client_config);
                client.expiration_gps = credential.expirationGps;
                new_s3_client = true;
            }
        }
        else
        {
            mlog(CRITICAL, "Credentials have expired for asset: %s (%.3lf)\n", asset->getName(), time_to_live);
        }
    }
    else if(client.s3_client == NULL)
    {
        /* Create S3 Client Configuration */
        Aws::Client::ClientConfiguration client_config;
        client_config.endpointOverride = asset->getEndpoint();
        client_config.region = asset->getRegion();

        /* Create S3 Client */
        client.s3_client = new Aws::S3::S3Client(client_config);
        new_s3_client = true;
    }

    /* Register New Client */
    if(new_s3_client)
    {
        clientsMut.lock();
        {
            clients.add(asset->getName(), client);
        }
        clientsMut.unlock();
    }

    /* Return S3 Client */
    return client.s3_client;
}
