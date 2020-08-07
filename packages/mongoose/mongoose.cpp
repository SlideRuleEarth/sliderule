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

#include "core.h"
#include "mongoose.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_MONGOOSE_LIBNAME    "mongoose"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * mongoose_open
 *----------------------------------------------------------------------------*/
int mongoose_open (lua_State *L)
{
    static const struct luaL_Reg mongoose_functions[] = {
        {"server",      RestServer::luaCreate},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, mongoose_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initmongoose (void)
{
    /* Install mongoose Package into Lua */
    LuaEngine::extend(LUA_MONGOOSE_LIBNAME, mongoose_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_MONGOOSE_LIBNAME, BINID);

    /* Display Status */
    printf("%s plugin initialized (%s)\n", LUA_MONGOOSE_LIBNAME, BINID);
}

void deinitmongoose (void)
{
}
}
