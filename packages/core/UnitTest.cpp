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
 * INCLUDES
 ******************************************************************************/

#include <stdlib.h>
#include "UnitTest.h"
#include "OsApi.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UnitTest::OBJECT_TYPE = "UnitTest";

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UnitTest::UnitTest (lua_State* L, const char* meta_name, const struct luaL_Reg* meta_table):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
    failures(0)
{
}

/*--------------------------------------------------------------------------------------
 * _ut_initialize - called via ut_initialize macro
 *--------------------------------------------------------------------------------------*/
bool UnitTest::_ut_initialize(void)
{
    lock.lock();
    {
        failures = 0;
    }
    lock.unlock();
    return true;
}

/*--------------------------------------------------------------------------------------
 * _ut_assert - called via ut_assert macro
 *--------------------------------------------------------------------------------------*/
bool UnitTest::_ut_assert(bool e, const char* file, int line, const char* fmt, ...)
{
    if(!e)
    {
        char formatted_string[UT_MAX_ASSERT];
        char log_message[UT_MAX_ASSERT];
        va_list args;
        int vlen, msglen;
        char* pathptr;

        /* Build Formatted String */
        va_start(args, fmt);
        vlen = vsnprintf(formatted_string, UT_MAX_ASSERT - 1, fmt, args);
        msglen = vlen < UT_MAX_ASSERT - 1 ? vlen : UT_MAX_ASSERT - 1;
        va_end(args);
        if (msglen < 0) formatted_string[0] = '\0';
        else            formatted_string[msglen] = '\0';

        /* Chop Path in Filename */
        pathptr = StringLib::find(file, '/', false);
        if(pathptr) pathptr++;
        else pathptr = const_cast<char*>(file);

        /* Create Log Message */
        msglen = snprintf(log_message, UT_MAX_ASSERT, "Failure at %s:%d:%s", pathptr, line, formatted_string);
        if(msglen > (UT_MAX_ASSERT - 1))
        {
            log_message[UT_MAX_ASSERT - 1] = '#';
        }

        /* Display Log Message */
        print2term("%s", log_message);

        /* Count Error */
        lock.lock();
        {
            failures++;
        }
        lock.unlock();
    }

    return e;
}

/*--------------------------------------------------------------------------------------
 * _ut_status - called via ut_status macro
 *--------------------------------------------------------------------------------------*/
bool UnitTest::_ut_status(void)
{
    bool status;
    lock.lock();
    {
        status = failures == 0;
    }
    lock.unlock();
    return status;
}
