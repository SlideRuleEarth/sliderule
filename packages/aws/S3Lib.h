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

#ifndef __s3_lib__
#define __s3_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <lua.h>
#include "OsApi.h"
#include "Dictionary.h"
#include "Ordering.h"

/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/

class S3Lib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* DEFAULT_ENDPOINT;
        static const char* DEFAULT_REGION;

        static const char* DEFAULT_CACHE_ROOT;
        static const int DEFAULT_MAX_CACHE_FILES = 16;
        
        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     init            (void);
        static void     deinit          (void);

        static bool     get             (const char* bucket, const char* key, const char** file);
        static int64_t  rangeGet        (uint8_t* data, int64_t size, uint64_t pos, const char* bucket, const char* key);

        static int      luaGet          (lua_State* L);
        static int      luaConfig       (lua_State* L);
        static int      luaCreateCache  (lua_State* L);
        static int      luaFlushCache   (lua_State* L);

    private:
        
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* endpoint;
        static const char* region;

        static const char* cacheRoot;
        static int cacheMaxSize;
        static Mutex cacheMut;
        static okey_t cacheIndex;
        static Dictionary<okey_t> cacheLookUp;
        static MgOrdering<const char*, okey_t, true> cacheFiles;
};

#endif  /* __s3_lib__ */
