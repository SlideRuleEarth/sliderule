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
#include "UT_Ordering.h"
#include "core.h"

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define ut_assert(e,...)    \
    try { \
        UT_Ordering::_ut_assert(e,__FILE__,__LINE__,__VA_ARGS__); \
    } catch(const RunTimeException& x) { \
        print2term("Caught exception: %s\n", x.what()); \
        UT_Ordering::_ut_assert(false,__FILE__,__LINE__,__VA_ARGS__); \
    };

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Ordering::TYPE = "UT_Ordering";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_Ordering::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_Ordering(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_Ordering::UT_Ordering(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("ADD_REMOVE", (cmdFunc_t)&UT_Ordering::testAddRemove,  0, "");
    registerCommand("DUPLICATES", (cmdFunc_t)&UT_Ordering::testDuplicates, 0, "");
    registerCommand("SORT",       (cmdFunc_t)&UT_Ordering::testSort,       0, "");
    registerCommand("ITERATE",    (cmdFunc_t)&UT_Ordering::testIterator,   0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Ordering::~UT_Ordering(void)
{
}

/*--------------------------------------------------------------------------------------
 * _ut_assert - called via ut_assert macro
 *--------------------------------------------------------------------------------------*/
bool UT_Ordering::_ut_assert(bool e, const char* file, int line, const char* fmt, ...)
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
 * testAddRemove
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testAddRemove(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;
    Ordering<int,int> mylist;

    // add initial set
    for(int i = 0; i < 75; i++)
    {
        mylist.add(i,i);
    }

    // check size
    ut_assert(mylist.length() == 75, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 75; i++)
    {
        ut_assert(mylist[i] == i, "failed to add %d\n", i);
    }

    // remove a handful of items
    mylist.remove(66);
    mylist.remove(55);
    mylist.remove(44);
    mylist.remove(33);
    mylist.remove(22);
    mylist.remove(11);
    mylist.remove(0);

    // check new size
    ut_assert(mylist.length() == 68, "failed length check %d\n", mylist.length());

    // check final set
    for(int i =  1; i < 11; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 12; i < 22; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 23; i < 33; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 34; i < 44; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 45; i < 55; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 56; i < 66; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 67; i < 75; i++) ut_assert(mylist[i] == i, "failed to keep %d\n", i);

    // return success or failure
    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testDuplicates(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;
    Ordering<int,int> mylist;

    // add initial set
    for(int i = 0; i < 20; i++)
    {
        mylist.add(i,i);
        mylist.add(i,i);
    }

    // check size
    ut_assert(mylist.length() == 40, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 20; i++)
    {
        ut_assert(mylist[i] == i, "failed to add %d\n", i);
        mylist.remove(i);
        ut_assert(mylist[i] == i, "failed to add %d\n", i);
    }

    // return success or failure
    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testSort
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testSort(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;

    // in order
    Ordering<int,int> mylist1;
    for(int i = 0; i < 20; i++)mylist1.add(i, i);
    for(int i = 0; i < 20; i++) ut_assert(mylist1[i] == i, "failed to sort %d\n", i);

    // reverse order
    Ordering<int,int> mylist2;
    for(int i = 0; i < 20; i++)
    {
        int d = 20 - i;
        mylist2.add(20 - i, d);
    }
    for(int i = 1; i <= 20; i++) ut_assert(mylist2[i] == i, "failed to sort %d\n", i);

    // random order
    Ordering<int,int> mylist3;
    int d;
    d = 19; mylist3.add(19, d);
    d = 1; mylist3.add(1, d);
    d = 2; mylist3.add(2, d);
    d = 5; mylist3.add(5, d);
    d = 4; mylist3.add(4, d);
    d = 18; mylist3.add(18, d);
    d = 13; mylist3.add(13, d);
    d = 14; mylist3.add(14, d);
    d = 15; mylist3.add(15, d);
    d = 11; mylist3.add(11, d);
    d = 3; mylist3.add(3, d);
    d = 6; mylist3.add(6, d);
    d = 8; mylist3.add(8, d);
    d = 7; mylist3.add(7, d);
    d = 9; mylist3.add(9, d);
    d = 12; mylist3.add(12, d);
    d = 10; mylist3.add(10, d);
    d = 17; mylist3.add(17, d);
    d = 16; mylist3.add(16, d);
    d = 0; mylist3.add(0, d);
    for(int i = 0; i < 20; i++) ut_assert(mylist3[i] == i, "failed to sort %d\n", i);

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testIterator
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testIterator(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;

    Ordering<int,int> mylist;

    for(int i = 0; i < 20; i++)
    {
        int d = 20 - i;
        mylist.add(20 - i, d);
    }

    Ordering<int,int>::Iterator iterator(mylist);
    for(int i = 0; i < 20; i++)
    {
        ut_assert(iterator[i].key == (i + 1), "failed to iterate key %d\n", i + 1);
        ut_assert(iterator[i].value == (i + 1), "failed to iterate value %d\n", i + 1);
    }

    return failures == 0 ? 0 : -1;
}
