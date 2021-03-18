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
/*
 * The S3 Client is not a part of the static member data of the S3Lib struct
 * because that would expose the S3Lib.h header file to the aws header space
 */ 
Aws::S3::S3Client* s3Client;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3Lib::AWS_S3_ENDPOINT_ENV_VAR_NAME = "SLIDERULE_S3_ENDPOINT";
const char* S3Lib::AWS_S3_REGION_ENV_VAR_NAME = "SLIDERULE_S3_REGION";

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
    /* Attempt to AWS S3 Information from Environment */
    const char* endpoint_from_env = getenv(AWS_S3_ENDPOINT_ENV_VAR_NAME);
    const char* region_from_env = getenv(AWS_S3_REGION_ENV_VAR_NAME);

    /* Set AWS Attributes */
    endpoint = endpoint_from_env ? StringLib::duplicate(endpoint_from_env) : StringLib::duplicate(DEFAULT_ENDPOINT);
    region = region_from_env ? StringLib::duplicate(region_from_env) : StringLib::duplicate(DEFAULT_REGION);

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
    mlog(INFO, "S3 %s object %s in bucket %s: %s", found_in_cache ? "cache hit on" : "download of", key, bucket, cache_filepath.getString());

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
        mlog(CRITICAL, "Failed to transfer S3 object: %d", (int)transfer_status);
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
    SafeString s3_rqst_range("bytes=%lu-%lu", (unsigned long)pos, (unsigned long)(pos + size - 1));
    object_request.SetRange(s3_rqst_range.getString());

    /* Make Request */
    Aws::S3::Model::GetObjectOutcome response = s3Client->GetObject(object_request);
    if(response.IsSuccess())
    {
        bytes_read = (int64_t)response.GetResult().GetContentLength();
        std::streambuf* sbuf = response.GetResult().GetBody().rdbuf();
        std::istream reader(sbuf);
        reader.read((char*)data, bytes_read);
    }
    else
    {
        throw RunTimeException("failed to read S3 data");
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
        mlog(CRITICAL, "Error getting S3 object: %s", e.what());
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
        mlog(CRITICAL, "Error configuring S3 access: %s", e.what());
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
                            mlog(CRITICAL, "Caching %s for S3 retrieval", key.getString());
                        }
                    }
                }
                closedir(dir);

                /* Log Status */
                if(file_count > 0)
                {
                    mlog(CRITICAL, "Loaded %ld of %d files into S3 cache", cacheFiles.length(), file_count);
                }
            }
        }
        cacheMut.unlock();
        
        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating S3 cache: %s", e.what());
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
            mlog(CRITICAL, "Flushing %ld files out of S3 cache", cacheFiles.length());
            cacheLookUp.clear();
            cacheFiles.clear();
        }
        cacheMut.unlock();
        
        /* Get Object and Write to File */
        return LuaObject::returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error flushing S3 cache: %s", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}
