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

#ifndef __s3_cache_io_driver__
#define __s3_cache_io_driver__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "LuaEngine.h"
#include "Ordering.h"
#include "Dictionary.h"

/******************************************************************************
 * S3 CACHE IO DRIVER CLASS
 ******************************************************************************/

class S3CacheIODriver: Asset::IODriver
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* FORMAT;

        static const char* DEFAULT_CACHE_ROOT;
        static const int DEFAULT_MAX_CACHE_FILES = 16;
        static const int FILE_BUFFER_SIZE = 0x1000000; // 16MB

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init            (void);
        static IODriver*    create          (const Asset* _asset);
        static int          luaCreateCache  (lua_State* L);
        static int          createCache     (const char* cache_root=DEFAULT_CACHE_ROOT, int max_files=DEFAULT_MAX_CACHE_FILES);

        void                ioOpen          (const char* resource);
        void                ioClose         (void);
        int64_t             ioRead          (uint8_t* data, int64_t size, uint64_t pos);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                S3CacheIODriver     (const Asset* _asset);
                ~S3CacheIODriver    (void);

        bool    fileGet             (const char* bucket, const char* key, const char** file);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char*                              cacheRoot;
        static int                                      cacheMaxSize;
        static Mutex                                    cacheMut;
        static okey_t                                   cacheIndex;
        static Dictionary<okey_t>                       cacheLookUp;
        static MgOrdering<const char*, okey_t, true>    cacheFiles;

        const Asset*    asset;
        fileptr_t       ioFile;
};

#endif  /* __s3_cache_io_driver__ */
