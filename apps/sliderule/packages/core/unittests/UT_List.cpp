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
#include "UnitTest.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_List::LUA_META_NAME = "UT_List";
const struct luaL_Reg UT_List::LUA_META_TABLE[] = {
    {"addremove",   testAddRemove},
    {"duplicates",  testDuplicates},
    {"sort",        testSort},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_List::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_List(L));
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
UT_List::UT_List (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_List::~UT_List(void) = default;

/*--------------------------------------------------------------------------------------
 * testAddRemove
 *--------------------------------------------------------------------------------------*/
int UT_List::testAddRemove(lua_State* L)
{
    UT_List* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_List*>(getLuaSelf(L, 1));        
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    List<int> mylist(10);

    // add initial set
    for(int i = 0; i < 75; i++)
    {
        mylist.add(i);
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
    int j = 0;
    for(int i =  1; i < 11; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 12; i < 22; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 23; i < 33; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 34; i < 44; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 45; i < 55; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 56; i < 66; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);
    for(int i = 67; i < 75; i++) ut_assert(lua_obj, mylist[j++] == i, "failed to keep %d\n", i);

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_List::testDuplicates(lua_State* L)
{
    UT_List* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_List*>(getLuaSelf(L, 1));        
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    List<int> mylist(10);

    // add initial set
    for(int i = 0; i < 20; i++)
    {
        mylist.add(i);
        mylist.add(i);
    }

    // check size
    ut_assert(lua_obj, mylist.length() == 40, "failed length check %d\n", mylist.length());

    // check initial set
    for(int i = 0; i < 20; i++)
    {
        ut_assert(lua_obj, mylist[i*2] == i, "failed to add %d\n", i);
        ut_assert(lua_obj, mylist[i*2+1] == i, "failed to add %d\n", i);
    }

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testSort
 *--------------------------------------------------------------------------------------*/
int UT_List::testSort(lua_State* L)
{
    UT_List* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_List*>(getLuaSelf(L, 1));        
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    // in order
    List<int> mylist1(10);
    for(int i = 0; i < 20; i++) mylist1.add(i);
    mylist1.sort();
    for(int i = 0; i < 20; i++) ut_assert(lua_obj, mylist1[i] == i, "failed to sort %d\n", i);

    // reverse order
    List<int> mylist2(10);
    for(int i = 0; i < 20; i++) mylist2.add(20 - i);
    mylist2.sort();
    for(int i = 0; i < 20; i++) ut_assert(lua_obj, mylist2[i] == (i + 1), "failed to sort %d\n", i + 1);

    // random order
    List<int> mylist3(10);
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
    for(int i = 0; i < 20; i++) ut_assert(lua_obj, mylist3[i] == i, "failed to sort %d\n", i);

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}
