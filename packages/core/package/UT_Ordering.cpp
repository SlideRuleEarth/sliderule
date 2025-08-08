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

#include "UT_Ordering.h"
#include "Ordering.h"
#include "UnitTest.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Ordering::LUA_META_NAME = "UT_Ordering";
const struct luaL_Reg UT_Ordering::LUA_META_TABLE[] = {
    {"addremove",   testAddRemove},
    {"duplicates",  testDuplicates},
    {"sort",        testSort},
    {"iterator",    testIterator},
    {"assignment",  testAssignment},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_Ordering::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_Ordering(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_Ordering::UT_Ordering (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*--------------------------------------------------------------------------------------
 * testAddRemove
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testAddRemove(lua_State* L)
{
    UT_Ordering* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Ordering*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    Ordering<int,int> mylist;

    // add initial set
    for(int i = 0; i < 75; i++)
    {
        mylist.add(i,i);
    }

    // check size
    ut_assert(lua_obj, mylist.length() == 75, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 75; i++)
    {
        ut_assert(lua_obj, mylist[i] == i, "failed to add %d\n", i);
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
    ut_assert(lua_obj, mylist.length() == 68, "failed length check %d\n", mylist.length());

    // check final set
    for(int i =  1; i < 11; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 12; i < 22; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 23; i < 33; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 34; i < 44; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 45; i < 55; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 56; i < 66; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);
    for(int i = 67; i < 75; i++) ut_assert(lua_obj, mylist[i] == i, "failed to keep %d\n", i);

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testDuplicates(lua_State* L)
{
    UT_Ordering* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Ordering*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    Ordering<int,int> mylist;

    // add initial set
    for(int i = 0; i < 20; i++)
    {
        mylist.add(i,i);
        mylist.add(i,i);
    }

    // check size
    ut_assert(lua_obj, mylist.length() == 40, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 20; i++)
    {
        ut_assert(lua_obj, mylist[i] == i, "failed to add %d\n", i);
        mylist.remove(i);
        ut_assert(lua_obj, mylist[i] == i, "failed to add %d\n", i);
    }

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testSort
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testSort(lua_State* L)
{
    UT_Ordering* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Ordering*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    // in order
    Ordering<int,int> mylist1;
    for(int i = 0; i < 20; i++)mylist1.add(i, i);
    for(int i = 0; i < 20; i++) ut_assert(lua_obj, mylist1[i] == i, "failed to sort %d\n", i);

    // reverse order
    Ordering<int,int> mylist2;
    for(int i = 0; i < 20; i++)
    {
        const int d = 20 - i;
        mylist2.add(20 - i, d);
    }
    for(int i = 1; i <= 20; i++) ut_assert(lua_obj, mylist2[i] == i, "failed to sort %d\n", i);

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
    for(int i = 0; i < 20; i++) ut_assert(lua_obj, mylist3[i] == i, "failed to sort %d\n", i);

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testIterator
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testIterator(lua_State* L)
{
    UT_Ordering* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Ordering*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    Ordering<int,int> mylist;

    for(int i = 0; i < 20; i++)
    {
        const int d = 20 - i;
        mylist.add(20 - i, d);
    }

    const Ordering<int,int>::Iterator iterator(mylist);
    for(int i = 0; i < 20; i++)
    {
        ut_assert(lua_obj, iterator[i].key == (i + 1), "failed to iterate key %d\n", i + 1);
        ut_assert(lua_obj, iterator[i].value == (i + 1), "failed to iterate value %d\n", i + 1);
    }

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testAssignment
 *--------------------------------------------------------------------------------------*/
int UT_Ordering::testAssignment(lua_State* L)
{
    UT_Ordering* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Ordering*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);
    Ordering<int,int> mylist;

    // add initial set
    for(int i = 0; i < 75; i++)
    {
        mylist.add(i,i);
    }

    // remove a handful of items
    mylist.remove(66);
    mylist.remove(55);
    mylist.remove(44);
    mylist.remove(33);
    mylist.remove(22);
    mylist.remove(11);
    mylist.remove(0);

    // assign / copy to another list
    Ordering<int,int> copiedlist;
    copiedlist = mylist;

    // check new size
    ut_assert(lua_obj, copiedlist.length() == 68, "failed length check %d\n", copiedlist.length());

    // check final set
    for(int i =  1; i < 11; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 12; i < 22; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 23; i < 33; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 34; i < 44; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 45; i < 55; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 56; i < 66; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);
    for(int i = 67; i < 75; i++) ut_assert(lua_obj, copiedlist[i] == i, "failed to keep %d\n", i);

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}
