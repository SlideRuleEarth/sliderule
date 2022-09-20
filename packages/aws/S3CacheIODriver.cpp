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

#include "S3CacheIODriver.h"
#include "OsApi.h"
#include "Asset.h"
#include "S3Lib.h"

#include <aws/core/Aws.h>
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/model/GetObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>

#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* S3CacheIODriver::FORMAT = "s3cache";

const char* S3CacheIODriver::DEFAULT_CACHE_ROOT = ".cache";
const char* S3CacheIODriver::cacheRoot = NULL;

int         S3CacheIODriver::cacheMaxSize = 0;
okey_t      S3CacheIODriver::cacheIndex = 0;
Mutex       S3CacheIODriver::cacheMut;

Dictionary<okey_t> S3CacheIODriver::cacheLookUp;
MgOrdering<const char*, okey_t, true> S3CacheIODriver::cacheFiles;

/******************************************************************************
 * FILE IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3CacheIODriver::init (void)
{
    cacheRoot = NULL;
    cacheMaxSize = DEFAULT_MAX_CACHE_FILES;
}
/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* S3CacheIODriver::create (const Asset* _asset)
{
    return new S3CacheIODriver(_asset);
}

/*----------------------------------------------------------------------------
 * luaCreateCache - s3cache(<root>, [<max_files>])
 *----------------------------------------------------------------------------*/
int S3CacheIODriver::luaCreateCache(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* cache_root  = LuaObject::getLuaString(L, 1, true, DEFAULT_CACHE_ROOT);
        int         max_files   = LuaObject::getLuaInteger(L, 2, true, DEFAULT_MAX_CACHE_FILES);

        /* Create Cache */
        createCache(cache_root, max_files);

        lua_pushboolean(L, true);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating S3 cache: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }
}

/*----------------------------------------------------------------------------
 * createCache
 *----------------------------------------------------------------------------*/
int S3CacheIODriver::createCache (const char* cache_root, int max_files)
{
    int file_count = 0;

    cacheMut.lock();
    {
        /* Create Cache Directory (if it doesn't exist) */
        int ret = mkdir(cache_root, 0700);
        if(ret == -1 && errno != EEXIST)
        {
            cacheMut.unlock();
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create cache directory %s: %s", cache_root, strerror(errno));
        }

        /* Set Cache Root */
        if(cacheRoot) delete [] cacheRoot;
        cacheRoot = StringLib::duplicate(cache_root);

        /* Set Maximum Number of Files */
        cacheMaxSize = max_files;

        /* Clear Out Cache Lookup Table and Files */
        cacheLookUp.clear();
        cacheFiles.clear();

        /* Traverse Directory and Build Cache (if it does exist) */
        DIR *dir;
        if((dir = opendir(cacheRoot)) != NULL)
        {
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
                        mlog(INFO, "Caching %s for S3 retrieval", key.getString());
                    }
                }
            }
            closedir(dir);

            /* Log Status */
            if(file_count > 0)
            {
                mlog(INFO, "Loaded %ld of %d files into S3 cache", cacheFiles.length(), file_count);
            }
        }
    }
    cacheMut.unlock();

    return file_count;
}

/*----------------------------------------------------------------------------
 * ioOpen
 *----------------------------------------------------------------------------*/
void S3CacheIODriver::ioOpen (const char* resource)
{
    /* Check if Cache Created */
    if(cacheRoot == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "cache has not been created yet");

    /* Create Path to Resource */
    SafeString resourcepath("%s/%s", asset->getPath(), resource);

    /* Allocate Bucket String */
    char* bucket = StringLib::duplicate(resourcepath.getString());

    /*
    * Differentiate Bucket and Key
    *  <bucket_name>/<path_to_file>/<filename>
    *  |             |
    * ioBucket      ioKey
    */
    char* key = bucket;
    while(*key != '\0' && *key != '/') key++;
    if(*key == '/')
    {
        /* Terminate Bucket and Set Key */
        *key = '\0';
        key++;

        /* Get Cached File */
        const char* filename = NULL;
        if(fileGet(bucket, key, &filename))
        {
            ioFile = fopen(filename, "r");
            delete [] filename;
        }
    }

    /* Free Bucket String */
    delete [] bucket;

    /* Check if File Opened */
    if(ioFile == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to open resource");
}

/*----------------------------------------------------------------------------
 * ioClose
 *----------------------------------------------------------------------------*/
void S3CacheIODriver::ioClose (void)
{
    if(ioFile) fclose(ioFile);
    ioFile = NULL;
}

/*----------------------------------------------------------------------------
 * ioRead
 *----------------------------------------------------------------------------*/
int64_t S3CacheIODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    /* Seek to New Position */
    if(fseek(ioFile, pos, SEEK_SET) != 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to go to I/O position: 0x%lx", pos);
    }

    /* Read Data */
    return fread(data, 1, size, ioFile);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
S3CacheIODriver::S3CacheIODriver (const Asset* _asset)
{
    asset = _asset;
    ioFile = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3CacheIODriver::~S3CacheIODriver (void)
{
    ioClose();
}

/*----------------------------------------------------------------------------
 * fileGet
 *----------------------------------------------------------------------------*/
bool S3CacheIODriver::fileGet (const char* bucket, const char* key, const char** file)
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
            cacheLookUp.add(key, cacheIndex);
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
    mlog(DEBUG, "S3 %s object %s in bucket %s: %s", found_in_cache ? "cache hit on" : "download of", key, bucket, cache_filepath.getString());

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

    /* Open Up File for Writing */
    FILE* fd = fopen(cache_filepath.getString(), "w");
    if(!fd) RunTimeException(CRITICAL, RTE_ERROR, "Unable to open file %s: %s", cache_filepath.getString(), LocalLib::err2str(errno));

    /* Set Bucket and Key */
    Aws::S3Crt::Model::GetObjectRequest object_request;
    object_request.SetBucket(bucket_name);
    object_request.SetKey(key_name);

    /* Make Request */
    S3Lib::client_t* client = S3Lib::createClient(asset);
    Aws::S3Crt::Model::GetObjectOutcome response = client->s3_client->GetObject(object_request);
    bool status = response.IsSuccess();

    /* Read Response */
    if(status)
    {
        int64_t total_bytes = (int64_t)response.GetResult().GetContentLength();
        std::streambuf* sbuf = response.GetResult().GetBody().rdbuf();
        std::istream reader(sbuf);
        char* data = new char [FILE_BUFFER_SIZE];

        /* Write File */
        int64_t bytes_read = 0;
        while(bytes_read < total_bytes)
        {
            int64_t bytes_left = total_bytes - bytes_read;
            int64_t bytes_to_read = MIN(bytes_left, FILE_BUFFER_SIZE);
            reader.read((char*)data, bytes_to_read);
            int64_t bytes_written = fwrite(data, bytes_to_read, 1, fd);
            if(bytes_written > 0)
            {
                bytes_read += bytes_written;
            }
            else
            {
                fclose(fd);
                RunTimeException(CRITICAL, RTE_ERROR, "Error(%d) writing file %s: %s", bytes_written, cache_filepath.getString(), LocalLib::err2str(errno));
            }
        }

        /* Clean Up File Data */
        delete [] data;
    }

    /* Clean Up File Descriptor */
    fclose(fd);

    /* Clean Up Client */
    S3Lib::destroyClient(client);

    /* Handle Errors or Return Bytes Read */
    if(!status)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read S3 data: %s", response.GetError().GetMessage().c_str());
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
