/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/


#include "S3Lib.h"
#include "core.h"

#include <aws/core/Aws.h>
#include <aws/transfer/TransferManager.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <assert.h>
#include <stdexcept>
#include <lua.h>


/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/

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
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
bool S3Lib::get (const char* bucket, const char* key, const char* file, const char* endpoint)
{
    (void)endpoint;

    const char* ALLOC_TAG = __FUNCTION__;

    const Aws::String bucket_name = bucket;
    const Aws::String key_name = key;
    const Aws::String file_name = file;

    /* Create S3 Client Configuration */
    Aws::Client::ClientConfiguration client_config;
    if(endpoint)
    {
        client_config.endpointOverride = endpoint;
        client_config.region = "US_WEST_2";
    }

    /* Create S3 Client */
    auto s3_client = Aws::MakeShared<Aws::S3::S3Client>(ALLOC_TAG, client_config);

    /* Create Transfer Configuration */
    auto thread_executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>(ALLOC_TAG, 4);
    Aws::Transfer::TransferManagerConfiguration transfer_config(thread_executor.get());
    transfer_config.s3Client = s3_client;

    /* Create Transfer Manager */
    auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);

    /* Download File */
    auto transfer_handle = transfer_manager->DownloadFile(bucket_name, key_name, file_name);

    /* Return Status */
    transfer_handle->WaitUntilFinished();
    auto transfer_status = transfer_handle->GetStatus();
    if(transfer_status != Aws::Transfer::TransferStatus::COMPLETED)
    {
        mlog(CRITICAL, "Failed to transfer S3 object: %d\n", (int)transfer_status);
        return false;
    }
    else
    {
        return true;
    }
}

/*----------------------------------------------------------------------------
 * luaGet - s3get(<filename>, <bucket>, <key>, [endpoint])
 *----------------------------------------------------------------------------*/
int S3Lib::luaGet(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* filename    = LuaObject::getLuaString(L, 1);
        const char* bucket      = LuaObject::getLuaString(L, 2);
        const char* key         = LuaObject::getLuaString(L, 3);
        const char* endpoint    = LuaObject::getLuaString(L, 4, true, NULL);

        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, get(bucket, key, filename, endpoint));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "ERror getting S3 object: %s\n", e.errmsg);
        return LuaObject::returnLuaStatus(L, false);
    }
}