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
#include "S3CurlIODriver.h"
#include "CredentialStore.h"
#include "OsApi.h"
#include "Asset.h"

#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

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
Asset::IODriver* S3CacheIODriver::create (const Asset* _asset, const char* resource)
{
    return new S3CacheIODriver(_asset, resource);
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
                        cacheLookUp.add(key.str(), cacheIndex);
                        const char* cache_key = StringLib::duplicate(key.str());
                        cacheFiles.add(cacheIndex, cache_key);
                        mlog(INFO, "Caching %s for S3 retrieval", key.str());
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
S3CacheIODriver::S3CacheIODriver (const Asset* _asset, const char* resource):
    S3CurlIODriver(_asset, resource)
{
    ioFile = NULL;

    /* Check if Cache Created */
    if(cacheRoot == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "cache has not been created yet");

    /* Get Cached File */
    const char* filename = NULL;
    if(fileGet(ioBucket, ioKey, &filename))
    {
        ioFile = fopen(filename, "r");
        delete [] filename;
    }

    /* Check if File Opened */
    if(ioFile == NULL)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to open resource");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3CacheIODriver::~S3CacheIODriver (void)
{
    if(ioFile) fclose(ioFile);
    ioFile = NULL;
}

/*----------------------------------------------------------------------------
 * fileGet
 *----------------------------------------------------------------------------*/
bool S3CacheIODriver::fileGet (const char* bucket, const char* key, const char** file)
{
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
    SafeString cache_filepath("%s%c%s", cacheRoot, PATH_DELIMETER, cache_filename.str());

    /* Log Operation */
    mlog(DEBUG, "S3 %s object %s in bucket %s: %s", found_in_cache ? "cache hit on" : "download of", key, bucket, cache_filepath.str());

    /* Quick Exit If Cache Hit */
    if(found_in_cache)
    {
        *file = cache_filepath.str(true);
        return true;
    }

    /* Download File */
    int64_t bytes_read = get(cache_filepath.str(), bucket, key, asset->getRegion(), &latestCredentials);
    if(bytes_read <= 0)
    {
        mlog(CRITICAL, "Failed to download S3 object: %ld", (long int)bytes_read);
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
                SafeString oldest_filepath("%s%c%s", cacheRoot, PATH_DELIMETER, oldest_filename.str());
                remove(oldest_filepath.str());
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
    *file = cache_filepath.str(true);
    return true;
}
