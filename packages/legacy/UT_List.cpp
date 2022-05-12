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
#include "UT_List.h"
#include "core.h"

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define ut_assert(e,...)    UT_List::_ut_assert(e,__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_List::TYPE = "UT_List";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_List::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_List(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_List::UT_List(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("ADD_REMOVE", (cmdFunc_t)&UT_List::testAddRemove,  0, "");
    registerCommand("DUPLICATES", (cmdFunc_t)&UT_List::testDuplicates, 0, "");
    registerCommand("SORT",       (cmdFunc_t)&UT_List::testSort,       0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_List::~UT_List(void)
{
}

/*--------------------------------------------------------------------------------------
 * _ut_assert - called via ut_assert macro
 *--------------------------------------------------------------------------------------*/
bool UT_List::_ut_assert(bool e, const char* file, int line, const char* fmt, ...)
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
int UT_List::testAddRemove(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;
    List<int, 10> mylist;

    // add initial set
    for(int i = 0; i < 75; i++)
    {
        mylist.add(i);
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
    int j = 0;
    for(int i =  1; i < 11; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 12; i < 22; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 23; i < 33; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 34; i < 44; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 45; i < 55; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 56; i < 66; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 67; i < 75; i++) ut_assert(mylist[j++] == i, "failed to keep %d\n", i);

    // return success or failure
    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_List::testDuplicates(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;
    List<int, 10> mylist;

    // add initial set
    for(int i = 0; i < 20; i++)
    {
        mylist.add(i);
        mylist.add(i);
    }

    // check size
    ut_assert(mylist.length() == 40, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 20; i++)
    {
        ut_assert(mylist[i*2] == i, "failed to add %d\n", i);
        ut_assert(mylist[i*2+1] == i, "failed to add %d\n", i);
    }

    // return success or failure
    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testSort
 *--------------------------------------------------------------------------------------*/
int UT_List::testSort(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    failures = 0;

    // in order
    List<int, 10> mylist1;
    for(int i = 0; i < 20; i++) mylist1.add(i);
    mylist1.sort();
    for(int i = 0; i < 20; i++) ut_assert(mylist1[i] == i, "failed to sort %d\n", i);

    // reverse order
    List<int, 10> mylist2;
    for(int i = 0; i < 20; i++) mylist2.add(20 - i);
    mylist2.sort();
    for(int i = 0; i < 20; i++) ut_assert(mylist2[i] == (i + 1), "failed to sort %d\n", i + 1);

    // random order
    List<int, 10> mylist3;
    mylist3.add(19);
    mylist3.add(1);
    mylist3.add(2);
    mylist3.add(5);
    mylist3.add(4);
    mylist3.add(18);
    mylist3.add(13);
    mylist3.add(14);
    mylist3.add(15);
    mylist3.add(11);
    mylist3.add(3);
    mylist3.add(6);
    mylist3.add(8);
    mylist3.add(7);
    mylist3.add(9);
    mylist3.add(12);
    mylist3.add(10);
    mylist3.add(17);
    mylist3.add(16);
    mylist3.add(0);
    mylist3.sort();
    for(int i = 0; i < 20; i++) ut_assert(mylist3[i] == i, "failed to sort %d\n", i);

    return failures == 0 ? 0 : -1;
}
