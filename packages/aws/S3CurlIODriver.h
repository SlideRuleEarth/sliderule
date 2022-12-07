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

#ifndef __s3_curl_io_driver__
#define __s3_curl_io_driver__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "Asset.h"
#include "CredentialStore.h"

/******************************************************************************
 * AWS S3 CLIENT CLASS
 ******************************************************************************/

class S3CurlIODriver: public Asset::IODriver
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long CONNECTION_TIMEOUT = 5; // seconds
        static const long READ_TIMEOUT = 600; // seconds
        static const long LOW_SPEED_LIMIT = 32768; // 32 KB/s
        static const long LOW_SPEED_TIME = 5; // seconds
        static const long ATTEMPTS_PER_REQUEST = 3;
        static const long SSL_VERIFYPEER = 0;
        static const long SSL_VERIFYHOST = 0;
        static const char* DEFAULT_REGION;
        static const char* DEFAULT_ASSET_NAME;
        static const char* FORMAT;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static IODriver*    create          (const Asset* _asset, const char* resource);
        virtual int64_t     ioRead          (uint8_t* data, int64_t size, uint64_t pos) override;

        static int          luaGet          (lua_State* L);
        static int          luaDownload     (lua_State* L);
        static int          luaRead         (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            S3CurlIODriver      (const Asset* _asset);
                            S3CurlIODriver      (const Asset* _asset, const char* resource);
        virtual             ~S3CurlIODriver     (void);

        // fixed GET - memory preallocated
        static int64_t      get                 (uint8_t* data, int64_t size, uint64_t pos,
                                                 const char* bucket, const char* key, const char* region,
                                                 CredentialStore::Credential* credentials);

        // streaming GET - memory allocated and returned
        static int64_t      get                 (uint8_t** data,
                                                 const char* bucket, const char* key, const char* region,
                                                 CredentialStore::Credential* credentials);

        // file GET - data written directly to file
        static int64_t      get                 (const char* filename,
                                                 const char* bucket, const char* key, const char* region,
                                                 CredentialStore::Credential* credentials);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const Asset*                asset;
        CredentialStore::Credential latestCredentials;
        char*                       ioBucket;
        char*                       ioKey;
};

#endif  /* __s3_curl_io_driver__ */
