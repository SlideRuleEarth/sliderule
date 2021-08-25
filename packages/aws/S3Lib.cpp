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
Dictionary<S3Lib::client_t*> S3Lib::clients(STARTING_NUM_CLIENTS);

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
        client_t* client;
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
 * createClient
 *----------------------------------------------------------------------------*/
S3Lib::client_t* S3Lib::createClient (const Asset* asset)
{
    client_t* client = NULL;

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
            (client->credential.provided && (client->credential.expirationGps < latest_credential.expirationGps)) ) // existing client has outdated credentials
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

            /* Create S3 Client Configuration */
            Aws::Client::ClientConfiguration client_config;
            client_config.endpointOverride = asset->getEndpoint();
            client_config.region = asset->getRegion();

            /* Create S3 Client */
            if(client->credential.provided)
            {
                const Aws::String accessKeyId(client->credential.accessKeyId);
                const Aws::String secretAccessKey(client->credential.secretAccessKey);
                const Aws::String sessionToken(client->credential.sessionToken);
                Aws::Auth::AWSCredentials awsCredentials(accessKeyId, secretAccessKey, sessionToken);
                client->s3_client = new Aws::S3::S3Client(awsCredentials, client_config);
            }
            else
            {
                client->s3_client = new Aws::S3::S3Client(client_config);
            }

            /* Register New Client */
            clients.add(asset->getName(), client);
        }
    }
    clientsMut.unlock();

    /* Return Client */
    return client;
}

/*----------------------------------------------------------------------------
 * destroyClient
 *----------------------------------------------------------------------------*/
void S3Lib::destroyClient (S3Lib::client_t* client)
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
            delete client;
        }
    }
    clientsMut.unlock();
}