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
#include "UT_String.h"
#include "core.h"

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define ut_assert(e,...)    UT_String::_ut_assert(e,__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_String::TYPE = "UT_String";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_String::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_String(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_String::UT_String(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("REPLACEMENT", (cmdFunc_t)&UT_String::testReplace,  0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_String::~UT_String(void)
{
}

/*--------------------------------------------------------------------------------------
 * _ut_assert - called via ut_assert macro
 *--------------------------------------------------------------------------------------*/
bool UT_String::_ut_assert(bool e, const char* file, int line, const char* fmt, ...)
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
        else pathptr = (char*)file;

        /* Create Log Message */
        msglen = snprintf(log_message, UT_MAX_ASSERT, "Failure at %s:%d:%s", pathptr, line, formatted_string);
        if(msglen > (UT_MAX_ASSERT - 1))
        {
            log_message[UT_MAX_ASSERT - 1] = '#';
        }

        /* Display Log Message */
        print2term("%s", log_message);

        /* Count Error */
        failures++;
    }

    return e;
}

/*--------------------------------------------------------------------------------------
 * testReplace
 *--------------------------------------------------------------------------------------*/
int UT_String::testReplace(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;

    // 1) Replace Single Character
    SafeString test1("Hello World");
    test1.replace("o", "X");
    ut_assert(StringLib::match(test1.getString(), "HellX WXrld"), "Failed single character test: %s", test1.getString());

    // 2) Replace String
    SafeString test2("Hello World");
    test2.replace("ello", "eal");
    ut_assert(StringLib::match(test2.getString(), "Heal World"), "Failed to replace string: %s", test2.getString());

    // 3) Replace Strings
    SafeString test3("This is a long $1 and I am $2 sure if this $1 will work or $2");
    const char* oldtxt[2] = { "$1", "$2" };
    const char* newtxt[2] = { "sentence", "not" };
    test3.inreplace(oldtxt, newtxt, 2);
    ut_assert(StringLib::match(test3.getString(), "This is a long sentence and I am not sure if this sentence will work or not"), "Failed multiple replacements: %s", test3.getString());

    // return success or failure
    return failures == 0 ? 0 : -1;
}
