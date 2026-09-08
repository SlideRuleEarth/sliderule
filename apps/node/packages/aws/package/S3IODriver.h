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

#ifndef __s3_io_driver__
#define __s3_io_driver__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "CredentialStore.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>

/******************************************************************************
 * AWS S3 CLIENT CLASS
 ******************************************************************************/

class S3IODriver: public Asset::IODriver
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* DEFAULT_IDENTITY;
        static const char* DRIVER_FORMAT;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init            (void);
        static IODriver*    create          (const Asset* _asset, const char* resource);
        int64_t             ioRead          (uint8_t* data, int64_t size, uint64_t pos) override;
        string              path            (void) override;
        int64_t             size            (void) override;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit            S3IODriver  (const char* bucket, const char* key, const Aws::Auth::AWSCredentials& credentials, Aws::Client::ClientConfiguration& config);
                            ~S3IODriver (void) override = default;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

         Aws::S3::S3Client s3Client client;
         Aws::S3::Model::GetObjectRequest request;
};

#endif  /* __s3_io_driver__ */
