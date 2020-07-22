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
 *INCLUDES
 ******************************************************************************/

#include <aws/core/Aws.h>

#include "core.h"
#include "aws.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_AWS_LIBNAME  "aws"

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

Aws::SDKOptions options;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * aws_open
 *----------------------------------------------------------------------------*/
int aws_open (lua_State *L)
{
    static const struct luaL_Reg aws_functions[] = {
        {"s3get",       S3Lib::luaGet},
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
    /* Initialize AWS SDK */
    #ifdef ENABLE_AWS_LOGGING
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
    #endif
    Aws::InitAPI(options);

    /* Initialize Modules */
    S3Lib::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_AWS_LIBNAME, aws_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_AWS_LIBNAME, BINID);

    /* Display Status */
    printf("%s plugin initialized (%s)\n", LUA_AWS_LIBNAME, BINID);
}

void deinitaws (void)
{
    S3Lib::deinit();
    Aws::ShutdownAPI(options);
}
}
