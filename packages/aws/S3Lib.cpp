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

#include <lua.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

Aws::S3::S3Client* s3Client;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3Lib::DEFAULT_ENDPOINT = "https://s3.us-west-2.amazonaws.com";
const char* S3Lib::DEFAULT_REGION = "us-west-2";

const char* S3Lib::endpoint = NULL;
const char* S3Lib::region = NULL;

const char* S3Lib::DEFAULT_CACHE_ROOT = ".cache";
const char* S3Lib::cacheRoot = NULL;

int S3Lib::cacheMaxSize = 0;
okey_t S3Lib::cacheIndex = 0;
Mutex S3Lib::cacheMut;
Dictionary<okey_t> S3Lib::cacheLookUp;
MgOrdering<const char*, okey_t, true> S3Lib::cacheFiles;

/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3Lib::init (void)
{
    /* Set AWS Attributes */
    endpoint = StringLib::duplicate(DEFAULT_ENDPOINT);
    region = StringLib::duplicate(DEFAULT_REGION);

    /* Set Cache Attributes */
    cacheRoot = StringLib::duplicate(DEFAULT_CACHE_ROOT);
    cacheMaxSize = DEFAULT_MAX_CACHE_FILES;

    /* Create S3 Client Configuration */
    Aws::Client::ClientConfiguration client_config;
    client_config.endpointOverride = endpoint;
    client_config.region = region;

    /* Create S3 Client */
    s3Client = new Aws::S3::S3Client(client_config);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void S3Lib::deinit (void)
{
    delete s3Client;

    delete [] endpoint;
    delete [] region;

    delete [] cacheRoot;
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
bool S3Lib::get (const char* bucket, const char* key, const char** file)
{
    const char* ALLOC_TAG = __FUNCTION__;

    /* Check Cache */
    bool found_in_cache = false;
    cacheMut.lock();
    {
        if(cacheLookUp.find(key))
        {
            cacheIndex++;
            cacheFiles.remove(cacheLookUp[key]);
            const char* cache_key = StringLib::duplicate(key);
            cacheFiles.add(cacheIndex, cache_key);
            found_in_cache = true;
        }        
    }
    cacheMut.unlock();

    /* Build Cache Filename */
    SafeString cache_filename("%s", key);
    cache_filename.replace(PATH_DELIMETER_STR, "#");
    SafeString cache_filepath("%s%c%s", cacheRoot, PATH_DELIMETER, cache_filename.getString());

    /* Log Operation */
    mlog(INFO, "S3 %s object %s in bucket %s: %s\n", found_in_cache ? "cache hit on" : "download of", key, bucket, cache_filepath.getString());

    /* Quick Exit If Cache Hit */
    if(found_in_cache)
    {
        *file = cache_filepath.getString(true);
        return true;
    }

    /* Build AWS String Parameters */
    const Aws::String bucket_name = bucket;
    const Aws::String key_name = key;
    const Aws::String file_name = cache_filepath.getString();

    /* Create Transfer Configuration */
    auto thread_executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>(ALLOC_TAG, 4);
    Aws::Transfer::TransferManagerConfiguration transfer_config(thread_executor.get());
    std::shared_ptr<Aws::S3::S3Client> transfer_client(s3Client);
    transfer_config.s3Client = transfer_client;

    /* Create Transfer Manager */
    auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);

    /* Download File */
    auto transfer_handle = transfer_manager->DownloadFile(bucket_name, key_name, file_name);

    /* Wait for Download to Complete */
    transfer_handle->WaitUntilFinished();
    auto transfer_status = transfer_handle->GetStatus();
    if(transfer_status != Aws::Transfer::TransferStatus::COMPLETED)
    {
        mlog(CRITICAL, "Failed to transfer S3 object: %d\n", (int)transfer_status);
        return false;
    }

    /* Populate Cache */
    cacheMut.lock();
    {
        if(cacheLookUp.length() >= cacheMaxSize)
        {
            /* Get Oldest File from Cache */
            const char* oldest_key = NULL;
            okey_t index = cacheFiles.first(&oldest_key);
            if(oldest_key != NULL)
            {
                /* Delete File in Local File System */
                SafeString oldest_filename("%s", oldest_key);
                oldest_filename.replace(PATH_DELIMETER_STR, "#");
                SafeString oldest_filepath("%s%c%s", cacheRoot, PATH_DELIMETER, oldest_filename.getString());
                remove(oldest_filepath.getString());
                cacheFiles.remove(index);
            }
        }

        /* Add New File to Cache */
        cacheIndex++;
        cacheLookUp.add(key, cacheIndex);
        const char* cache_key = StringLib::duplicate(key);
        cacheFiles.add(cacheIndex, cache_key);
    }
    cacheMut.unlock();

    /* Return Success */
    *file = cache_filepath.getString(true);
    return true;
}

/*----------------------------------------------------------------------------
 * rangeGet
 *----------------------------------------------------------------------------*/
int64_t S3Lib::rangeGet (uint8_t* data, int64_t size, uint64_t pos, const char* bucket, const char* key)
{
    int64_t bytes_read = 0;
    Aws::S3::Model::GetObjectRequest object_request;
    
    /* Set Bucket and Key */
    object_request.SetBucket(bucket);
    object_request.SetKey(key);

    /* Set Range */
    SafeString s3_rqst_range("bytes=%lu-%lu", (unsigned long)pos, (unsigned long)(pos + size));
    object_request.SetRange(s3_rqst_range.getString());

    /* Make Request */
    auto response = s3Client->GetObject(object_request);
    if (response.IsSuccess()) 
    {
        bytes_read = size; // TODO - must get size of result
        LocalLib::copy(data, response.GetResult().GetBody().rdbuf(), bytes_read);
    }

    return bytes_read;
}

/*----------------------------------------------------------------------------
 * luaGet - s3get(<bucket>, <key>) -> filename
 *----------------------------------------------------------------------------*/
int S3Lib::luaGet(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* bucket      = LuaObject::getLuaString(L, 1);
        const char* key         = LuaObject::getLuaString(L, 2);

        /* Download File */
        const char* filename = NULL;
        bool status = get(bucket, key, &filename);
        if(status)
        {
            lua_pushstring(L, filename);
            delete [] filename;
        }
        else
        {
            lua_pushnil(L);
        }

        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, status, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error getting S3 object: %s\n", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaConfig - s3config(<endpoint>, <region>)
 * 
 *  Note: Should only be called once at start of program
 *----------------------------------------------------------------------------*/
int S3Lib::luaConfig(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* _endpoint   = LuaObject::getLuaString(L, 1);
        const char* _region     = LuaObject::getLuaString(L, 2);

        /* Change Endpoint and Region */
        delete [] endpoint;
        delete [] region;
        endpoint = StringLib::duplicate(_endpoint);
        endpoint = StringLib::duplicate(_region);
        
        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring S3 access: %s\n", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaCreateCache - s3cache(<root>, <max_files>)
 *----------------------------------------------------------------------------*/
int S3Lib::luaCreateCache(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* cache_root  = LuaObject::getLuaString(L, 1);
        int         max_files   = LuaObject::getLuaInteger(L, 2, true, DEFAULT_MAX_CACHE_FILES);

        /* Create Cache */
        cacheMut.lock();
        {
            /* Set Cache Root */
            if(cacheRoot) delete [] cacheRoot;
            cacheRoot = StringLib::duplicate(cache_root);

            /* Create Cache Directory (if it doesn't exist) */
            mkdir(cacheRoot, 0700);

            /* Set Maximum Number of Files */
            cacheMaxSize = max_files;

            /* Clear Out Cache Lookup Table and Files */
            cacheLookUp.clear();
            cacheFiles.clear();

            /* Traverse Directory and Build Cache (if it does exist) */
            DIR *dir;
            if((dir = opendir(cacheRoot)) != NULL)
            {
                int file_count = 0;
                struct dirent *ent;
                while((ent = readdir(dir)) != NULL)
                {
                    if(!StringLib::match(".", ent->d_name) && !StringLib::match("..", ent->d_name))
                    {
                        if(file_count++ < cacheMaxSize)
                        {
                            char cache_filepath[MAX_STR_SIZE];
                            StringLib::format(cache_filepath, MAX_STR_SIZE, "%s%c%s", cacheRoot, PATH_DELIMETER, ent->d_name);
                            
                            /* Reformat Filename to Key */
                            SafeString key("%s", ent->d_name);
                            key.replace("#", PATH_DELIMETER_STR);

                            /* Add File to Cache */
                            cacheIndex++;
                            cacheLookUp.add(key.getString(), cacheIndex);
                            const char* cache_key = StringLib::duplicate(key.getString());
                            cacheFiles.add(cacheIndex, cache_key);
                            mlog(CRITICAL, "Caching %s for S3 retrieval\n", key.getString());
                        }
                    }
                }
                closedir(dir);

                /* Log Status */
                if(file_count > 0)
                {
                    mlog(CRITICAL, "Loaded %ld of %d files into S3 cache\n", cacheFiles.length(), file_count);
                }
            }
        }
        cacheMut.unlock();
        
        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating S3 cache: %s\n", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaFlushCache - s3flush()
 *----------------------------------------------------------------------------*/
int S3Lib::luaFlushCache(lua_State* L)
{
    try
    {
        /* Flush Cache */
        cacheMut.lock();
        {
            mlog(CRITICAL, "Flushing %ld files out of S3 cache\n", cacheFiles.length());
            cacheLookUp.clear();
            cacheFiles.clear();
        }
        cacheMut.unlock();
        
        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error flushing S3 cache: %s\n", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}
