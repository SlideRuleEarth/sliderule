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

#include "core.h"
#include "h5.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_H5_LIBNAME  "h5"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * h5_open
 *----------------------------------------------------------------------------*/
int h5_open (lua_State *L)
{
    static const struct luaL_Reg h5_functions[] = {
        {"file",        H5File::luaCreate},
        {"dataset",     H5DatasetDevice::luaCreate},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, h5_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "ALL_ROWS", H5Coro::ALL_ROWS);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void inith5 (void)
{
    /* Initialize Modules */
    H5Coro::init();
    H5DatasetDevice::init();
    H5File::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_H5_LIBNAME, h5_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_H5_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_H5_LIBNAME, LIBID);
}

void deinith5 (void)
{
    H5Coro::deinit();
}
}
