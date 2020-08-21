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

        static void init            (void);
        static void deinit          (void);

        static bool get             (const char* bucket, const char* key, const char** file);

        static int  luaGet          (lua_State* L);
        static int  luaConfig       (lua_State* L);
        static int  luaCreateCache  (lua_State* L);
        static int  luaFlushCache   (lua_State* L);

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
