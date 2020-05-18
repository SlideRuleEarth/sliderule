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
#include "icesat2.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_ICESAT2_LIBNAME    "icesat2"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * icesat2_open
 *----------------------------------------------------------------------------*/
int icesat2_open (lua_State *L)
{
    static const struct luaL_Reg icesat2_functions[] = {
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, icesat2_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initicesat2 (void)
{
    LuaEngine::extend(LUA_ICESAT2_LIBNAME, icesat2_open);

	/* Display Status */
    printf("ICESat-2 Plugin Initialized (%s)\n", BINID);
}
}
