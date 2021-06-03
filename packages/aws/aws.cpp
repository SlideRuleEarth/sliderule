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
 *INCLUDES
 ******************************************************************************/

#include <aws/core/Aws.h>

#include "core.h"
#include "aws.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_AWS_LIBNAME     "aws"
#define S3_IO_DRIVER        "s3"
#define S3_CACHE_IO_DRIVER  "s3cache"

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

Aws::SDKOptions options;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * S3 I/O Driver
 *
 *  Notes: Used as driver for S3 access
 *----------------------------------------------------------------------------*/
class S3IODriver: Asset::IODriver
{
    public:

        static IODriver* create (const Asset* _asset)
        {
            return new S3IODriver(_asset);
        }

        void ioOpen (const char* resource)
        {
            SafeString resourcepath("%s/%s", asset->getUrl(), resource);

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
            else throw RunTimeException(CRITICAL, "invalid S3 url: %s", resource);
            ioKey++;
        }

        void ioClose (void)
        {
        }

        int64_t ioRead (uint8_t* data, int64_t size, uint64_t pos)
        {
            return S3Lib::rangeGet(data, size, pos, ioBucket, ioKey);
        }

    private:

        S3IODriver (const Asset* _asset)
        {
            asset = _asset;
            ioBucket = NULL; 
            ioKey = NULL;
        }

        ~S3IODriver (void)
        { 
            /*
             * Delete Memory Allocated for ioBucket
             *  only ioBucket is freed because ioKey only points
             *  into the memory allocated to ioBucket
             */
            if(ioBucket) delete [] ioBucket;
        }

        const Asset*    asset;
        char*           ioBucket;
        char*           ioKey;
};

/*----------------------------------------------------------------------------
 * S3 Cache I/O Driver
 *
 *  Notes: Used as driver for S3 cached access
 *----------------------------------------------------------------------------*/
class S3CacheIODriver: Asset::IODriver
{
    public:

        static IODriver* create (const Asset* _asset)
        {
            return new S3CacheIODriver(_asset);
        }

        void ioOpen (const char* resource)
        {
            SafeString resourcepath("%s/%s", asset->getUrl(), resource);

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
                if(S3Lib::fileGet(bucket, key, &filename))
                {
                    ioFile = fopen(filename, "r");
                    delete [] filename;
                }                
            }

            /* Free Bucket String */
            delete [] bucket;

            /* Check if File Opened */
            if(ioFile == NULL) throw RunTimeException(CRITICAL, "failed to open resource");
        }

        void ioClose (void)
        {
            if(ioFile) fclose(ioFile);
            ioFile = NULL;
        }

        int64_t ioRead (uint8_t* data, int64_t size, uint64_t pos)
        {
            /* Seek to New Position */
            if(fseek(ioFile, pos, SEEK_SET) != 0)
            {
                throw RunTimeException(CRITICAL, "failed to go to I/O position: 0x%lx", pos);
            }

            /* Read Data */
            return fread(data, 1, size, ioFile);            
        }

    private:

        S3CacheIODriver (const Asset* _asset)
        {
            asset = _asset;
            ioFile = NULL; 
        }

        ~S3CacheIODriver (void)
        { 
            ioClose(); 
        }

        const Asset*    asset;
        fileptr_t       ioFile;
};

/*----------------------------------------------------------------------------
 * aws_open
 *----------------------------------------------------------------------------*/
int aws_open (lua_State *L)
{
    static const struct luaL_Reg aws_functions[] = {
        {"csget",       CredentialStore::luaGet},
        {"csput",       CredentialStore::luaPut},
        {"s3get",       S3Lib::luaGet},
        {"s3config",    S3Lib::luaConfig},
        {"s3cache",     S3Lib::luaCreateCache},
        {"s3flush",     S3Lib::luaFlushCache},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, aws_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initaws (void)
{
    /* Configure AWS Logging */
    #ifdef ENABLE_AWS_LOGGING
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
    #endif

    /* Configure AWS to Handle Broken Pipes */
    options.httpOptions.installSigPipeHandler = true;

    /* Initialize AWS SDK */
    Aws::InitAPI(options);

    /* Initialize Modules */
    CredentialStore::init();
    S3Lib::init();
    Asset::registerDriver(S3_IO_DRIVER, S3IODriver::create);
    Asset::registerDriver(S3_CACHE_IO_DRIVER, S3CacheIODriver::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_AWS_LIBNAME, aws_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_AWS_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_AWS_LIBNAME, LIBID);
}

void deinitaws (void)
{
    S3Lib::deinit();
    CredentialStore::deinit();
    Aws::ShutdownAPI(options);
}
}
