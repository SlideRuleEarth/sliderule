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

#include "UT_Table.h"
#include "Table.h"
#include "UnitTest.h"
#include "TimeLib.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Table::LUA_META_NAME = "UT_Table";
const struct luaL_Reg UT_Table::LUA_META_TABLE[] = {
    {"addremove",   testAddRemove},
    {"chaining",    testChaining},
    {"removing",    testRemoving},
    {"duplicates",  testDuplicates},
    {"fulltable",   testFullTable},
    {"collisions",  testCollisions},
    {"stress",      testStress},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_Table::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_Table(L));
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
UT_Table::UT_Table (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*--------------------------------------------------------------------------------------
 * testAddRemove
 *--------------------------------------------------------------------------------------*/
int UT_Table::testAddRemove(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key, data;
    const int size = 8;
    Table<int,int> mytable(size);

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(key = 0; key < size; key++)
    {
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add entry %d\n", key);
    }

    /* Check Size */
    ut_assert(lua_obj, mytable.length() == 8, "Failed to get hash size of 8\n");

    /* Transverse Set */
    key = mytable.first(&data);
    while(key != (int)INVALID_KEY)
    {
        ut_assert(lua_obj, data == key, "Failed to get next key %d\n", key);
        ut_assert(lua_obj, mytable.remove(key), "Failed to remove key %d\n", key);
        ut_assert(lua_obj, mytable.length() == size - key - 1, "Failed to get size\n");
        key = mytable.first(&data);
    }

    /* Check Empty */
    ut_assert(lua_obj, mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(lua_obj, mytable.length() == 0, "Failed to remove all entries\n");

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testChaining
 *--------------------------------------------------------------------------------------*/
int UT_Table::testChaining(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key, data;
    const int size = 8;
    Table<int,int> mytable(size);
    const int test_data[8] = {0,1,2,3,8,9,10,11};

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)
    {
        key = mytable.first(&data);
        ut_assert(lua_obj, test_data[i] == key, "Failed to get next key %d\n", key);
        ut_assert(lua_obj, mytable.remove(key), "Failed to remove key %d\n", key);
        ut_assert(lua_obj, mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(lua_obj, mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(lua_obj, mytable.length() == 0, "Failed to remove all entries\n");

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testRemoving
 *--------------------------------------------------------------------------------------*/
int UT_Table::testRemoving(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key;
    int data;
    const int size = 16;
    Table<int,int> mytable(size);
    const int test_data[16]    = {0, 16, 32,  1, 17, 33,  2, 18, 34,  3,  4,  5,  6,  7,  8,  9};
    const int remove_order[16] = {0, 16, 32, 17, 33,  1, 34, 18,  2,  3,  4,  5,  6,  7,  8,  9};
    const int check_order[16]  = {0, 16, 32,  1,  1,  1,  2,  2,   2,  3,  4,  5,  6,  7,  8,  9};

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)
    {
        key = mytable.first(&data);
        ut_assert(lua_obj, check_order[i] == key, "Failed to get next key %d != %d, %d\n", check_order[i], key, i);
        ut_assert(lua_obj, mytable.remove(remove_order[i]), "Failed to remove key %d\n", remove_order[i]);
        ut_assert(lua_obj, mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(lua_obj, mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(lua_obj, mytable.length() == 0, "Failed to remove all entries\n");

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_Table::testDuplicates(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key;
    const int size = 16;
    Table<int,int> mytable(size);
    const int test_data[16] = {0,16,32,1,17,33,2,18,34,3,4,5,6,7,8,9};

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add key %d\n", key);
    }

    /* Add Duplicate Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to reject duplicate key %d\n", key);
    }

    /* Overwrite Duplicate Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, false), "Failed to overwrite duplicate key %d\n", key);
    }

    /* Add Rest of Set */
    for(int i = 9; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add key %d\n", key);
    }

    /* Overwrite Entire Duplicate Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, false), "Failed to overwrite duplicate key %d\n", key);
    }

    /* Attempt to Add to Full Hash */
    key = 35;
    ut_assert(lua_obj, mytable.add(key, key, false) == false, "Failed to detect full table\n");

    /* Check Size */
    ut_assert(lua_obj, mytable.length() == size, "Failed to rget size of table\n");

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testFullTable
 *--------------------------------------------------------------------------------------*/
int UT_Table::testFullTable(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key;
    const int size = 8;
    Table<int,int> mytable(size);
    const int test_data[8] = {0,1,2,3,4,5,6,7};

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add key %d\n", key);
    }

    /* Fail to Add on Full Table */
    key = 0; ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table, %d\n", key);
    key = 8; ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table, %d\n", key);
    key = 9; ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table, %d\n", key);

    /* Fail to Add on Changing Full Table */
    for(key = 0; key < size; key++)
    {
        ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table %d\n", key);
        ut_assert(lua_obj, mytable.remove(key), "Failed to remove key %d\n", key);

        ut_assert(lua_obj, mytable.add(key, key, true), "Failed to add key %d\n", key);

        const int new1_key = key + size;
        ut_assert(lua_obj, mytable.add(new1_key, new1_key, true) == false, "Failed to error on adding key to full table %d\n", new1_key);

        const int new2_key = key + size + 1;
        ut_assert(lua_obj, mytable.add(new2_key, new2_key, true) == false, "Failed to error on adding key to full table %d\n", new2_key);
    }

    /* Fail to Add on Overwritten Full Table */
    for(key = 0; key < size; key++)
    {
        ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table %d\n", key);
        ut_assert(lua_obj, mytable.add(key, key, false), "Failed to overwrite key %d\n", key);

        ut_assert(lua_obj, mytable.add(key, key, true) == false, "Failed to error on adding key to full table %d\n", key);

        const int new1_key = key + size;
        ut_assert(lua_obj, mytable.add(new1_key, new1_key, true) == false, "Failed to error on adding key to full table %d\n", new1_key);

        const int new2_key = key + size + 1;
        ut_assert(lua_obj, mytable.add(new2_key, new2_key, true) == false, "Failed to error on adding key to full table %d\n", new2_key);
    }

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testCollisions
 *--------------------------------------------------------------------------------------*/
int UT_Table::testCollisions(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key, data;
    const int size = 16;
    Table<int,int> mytable(size);
    const int test_data[16]    =  {0,16,32, 1,17,33, 2,18,34,40,50,66,48,35, 8, 9};
    const int remove_order[16] =  {0,16,32,17,33, 1,34,18, 2,40,50,66,48,35, 8, 9};
    const int check_order[16]  =  {0,16,32, 1, 1, 1, 2, 2, 2,40,50,66,48,35, 8, 9};

    ut_initialize(lua_obj);

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(lua_obj, mytable.add(key, key, false), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)
    {
        key = mytable.first(&data);
        ut_assert(lua_obj, check_order[i] == key, "Failed to get next key %d != %d\n", check_order[i], key);
        ut_assert(lua_obj, mytable.remove(remove_order[i]), "Failed to remove key %d\n", remove_order[i]);
        ut_assert(lua_obj, mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(lua_obj, mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(lua_obj, mytable.length() == 0, "Failed to remove all entries\n");

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*--------------------------------------------------------------------------------------
 * testStress
 *--------------------------------------------------------------------------------------*/
int UT_Table::testStress(lua_State* L)
{
    UT_Table* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Table*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    int key, data = 0;
    const int size = 64;
    int data_order[64];
    const int test_cycles = 65536;
    const int key_range = 0xFFFFFFFF;
    Table<int,int> mytable(size);

    ut_initialize(lua_obj);

    /* Seed Random Number */
    srand((unsigned int)TimeLib::latchtime());

    /* Cycle Tests */
    for(int j = 0; j < test_cycles; j++)
    {
        /* Reset Test */
        int num_added = 0;

        /* Load Hash */
        for(int i = 0; i < size; i++)
        {
            key = rand() % key_range; // NOLINT(concurrency-mt-unsafe)
            if(mytable.add(key, key, true))
            {
                data_order[num_added++] = key;
            }
        }

        /* Check Hash */
        for(int i = 0; i < num_added; i++)
        {
            key = data_order[i];

            mytable.first(&data);
            ut_assert(lua_obj, data == key, "Failed to get next key %d != %d\n", data, key);
            mytable.first(&data);
            ut_assert(lua_obj, data == key, "Failed to get same key %d != %d\n", data, key);
            ut_assert(lua_obj, mytable.remove(key), "Failed to remove key %d\n", key);
        }

        /* Check Empty */
        ut_assert(lua_obj, mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
        ut_assert(lua_obj, mytable.length() == 0, "Failed to remove all entries\n");
    }

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}
