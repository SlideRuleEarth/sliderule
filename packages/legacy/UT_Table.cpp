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
 * INCLUDES
 ******************************************************************************/

#include <stdlib.h>
#include "UT_Table.h"
#include "core.h"

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define ut_assert(e,...)    UT_Table::_ut_assert(e,__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Table::TYPE = "UT_Table";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_Table::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_Table(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_Table::UT_Table(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("ADD_REMOVE", (cmdFunc_t)&UT_Table::testAddRemove,  0, "");
    registerCommand("CHAINING",   (cmdFunc_t)&UT_Table::testChaining,   0, "");
    registerCommand("REMOVING",   (cmdFunc_t)&UT_Table::testRemoving,   0, "");
    registerCommand("DUPLICATES", (cmdFunc_t)&UT_Table::testDuplicates, 0, "");
    registerCommand("FULL_TABLE", (cmdFunc_t)&UT_Table::testFullTable,  0, "");
    registerCommand("COLLISIONS", (cmdFunc_t)&UT_Table::testCollisions, 0, "");
    registerCommand("STRESS",     (cmdFunc_t)&UT_Table::testStress,     0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Table::~UT_Table(void)
{
}

/*--------------------------------------------------------------------------------------
 * _ut_assert - called via ut_assert macro
 *--------------------------------------------------------------------------------------*/
bool UT_Table::_ut_assert(bool e, const char* file, int line, const char* fmt, ...)
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
        msglen = snprintf(log_message, UT_MAX_ASSERT, "Failure @ %s:%d:%s", pathptr, line, formatted_string);
        if(msglen > (UT_MAX_ASSERT - 1))
        {
            log_message[UT_MAX_ASSERT - 1] = '#';
        }

        /* Display Log Message */
        mlog(RAW, "%s", log_message);

        /* Count Error */
        failures++;
    }

    return e;
}

/*--------------------------------------------------------------------------------------
 * testAddRemove
 *--------------------------------------------------------------------------------------*/
int UT_Table::testAddRemove(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key, data;
    int size = 8;
    Table<int,int> mytable(size);

    failures = 0;

    /* Add Initial Set */
    for(key = 0; key < size; key++)
    {
        ut_assert(mytable.add(key, key, false), "Failed to add entry %d\n", key);
    }

    /* Check Size */
    ut_assert(mytable.length() == 8, "Failed to get hash size of 8\n");

    /* Transverse Set */
    key = mytable.first(&data);
    while(key != (int)INVALID_KEY)
    {
        ut_assert(data == key, "Failed to get next key %d\n", key);
        ut_assert(mytable.remove(key), "Failed to remove key %d\n", key);
        ut_assert(mytable.length() == size - key - 1, "Failed to get size\n");
        key = mytable.first(&data);
    }

    /* Check Empty */
    ut_assert(mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(mytable.length() == 0, "Failed to remove all entries\n");

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testChaining
 *--------------------------------------------------------------------------------------*/
int UT_Table::testChaining(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key, data;
    int size = 8;
    Table<int,int> mytable(size);
    int test_data[8] = {0,1,2,3,8,9,10,11};

    failures = 0;

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)    
    {
        key = mytable.first(&data);
        ut_assert(test_data[i] == key, "Failed to get next key %d\n", key);
        ut_assert(mytable.remove(key), "Failed to remove key %d\n", key);
        ut_assert(mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(mytable.length() == 0, "Failed to remove all entries\n");

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testRemoving
 *--------------------------------------------------------------------------------------*/
int UT_Table::testRemoving(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key, data;
    int size = 16;
    Table<int,int> mytable(size);
    int test_data[16]    = {0, 16, 32,  1, 17, 33,  2, 18, 34,  3,  4,  5,  6,  7,  8,  9};
    int remove_order[16] = {0, 16, 32, 17, 33,  1, 34, 18,  2,  3,  4,  5,  6,  7,  8,  9};
    int check_order[16]  = {0, 16, 32,  1,  1,  1,  2,  2,   2,  3,  4,  5,  6,  7,  8,  9};

    failures = 0;

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)    
    {
        key = mytable.first(&data);
        ut_assert(check_order[i] == key, "Failed to get next key %d != %d, %d\n", check_order[i], key, i);
        ut_assert(mytable.remove(remove_order[i]), "Failed to remove key %d\n", remove_order[i]);
        ut_assert(mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(mytable.length() == 0, "Failed to remove all entries\n");

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testDuplicates
 *--------------------------------------------------------------------------------------*/
int UT_Table::testDuplicates(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key;
    int size = 16;
    Table<int,int> mytable(size);
    int test_data[16] = {0,16,32,1,17,33,2,18,34,3,4,5,6,7,8,9};

    failures = 0;

    /* Add Initial Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add key %d\n", key);
    }

    /* Add Duplicate Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false) == false, "Failed to reject duplicate key %d\n", key);
    }

    /* Overwrite Duplicate Set */
    for(int i = 0; i < 9; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, true), "Failed to overwrite duplicate key %d\n", key);
    }

    /* Add Rest of Set */
    for(int i = 9; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add key %d\n", key);
    }

    /* Overwrite Entire Duplicate Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, true), "Failed to overwrite duplicate key %d\n", key);
    }

    /* Attempt to Add to Full Hash */
    key = 35;
    ut_assert(mytable.add(key, key, true) == false, "Failed to detect full table\n");

    /* Check Size */
    ut_assert(mytable.length() == size, "Failed to rget size of table\n");

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testFullTable
 *--------------------------------------------------------------------------------------*/
int UT_Table::testFullTable(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key;
    int size = 8;
    Table<int,int> mytable(size);
    int test_data[8] = {0,1,2,3,4,5,6,7};

    failures = 0;

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add key %d\n", key);
    }

    /* Fail to Add on Full Table */
    key = 0; ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table, %d\n", key);
    key = 8; ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table, %d\n", key);
    key = 9; ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table, %d\n", key);

    /* Fail to Add on Changing Full Table */
    for(key = 0; key < size; key++)
    {
        ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table %d\n", key);
        ut_assert(mytable.remove(key), "Failed to remove key %d\n", key);

        ut_assert(mytable.add(key, key, false), "Failed to add key %d\n", key);

        int new1_key = key + size;
        ut_assert(mytable.add(new1_key, new1_key, false) == false, "Failed to error on adding key to full table %d\n", new1_key);

        int new2_key = key + size + 1;
        ut_assert(mytable.add(new2_key, new2_key, false) == false, "Failed to error on adding key to full table %d\n", new2_key);
    }

    /* Fail to Add on Overwritten Full Table */
    for(key = 0; key < size; key++)
    {
        ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table %d\n", key);
        ut_assert(mytable.add(key, key, true), "Failed to overwrite key %d\n", key);

        ut_assert(mytable.add(key, key, false) == false, "Failed to error on adding key to full table %d\n", key);

        int new1_key = key + size;
        ut_assert(mytable.add(new1_key, new1_key, false) == false, "Failed to error on adding key to full table %d\n", new1_key);

        int new2_key = key + size + 1;
        ut_assert(mytable.add(new2_key, new2_key, false) == false, "Failed to error on adding key to full table %d\n", new2_key);
    }

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testCollisions
 *--------------------------------------------------------------------------------------*/
int UT_Table::testCollisions(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key, data;
    int size = 16;
    Table<int,int> mytable(size);
    int test_data[16]    =  {0,16,32, 1,17,33, 2,18,34,40,50,66,48,35, 8, 9};
    int remove_order[16] =  {0,16,32,17,33, 1,34,18, 2,40,50,66,48,35, 8, 9};
    int check_order[16]  =  {0,16,32, 1, 1, 1, 2, 2, 2,40,50,66,48,35, 8, 9};

    failures = 0;

    /* Add Initial Set */
    for(int i = 0; i < size; i++)
    {
        key = test_data[i];
        ut_assert(mytable.add(key, key, false), "Failed to add entry %d\n", key);
    }

    /* Transverse Set */
    for(int i = 0; i < size; i++)    
    {
        key = mytable.first(&data);
        ut_assert(check_order[i] == key, "Failed to get next key %d != %d\n", check_order[i], key);
        ut_assert(mytable.remove(remove_order[i]), "Failed to remove key %d\n", remove_order[i]);
        ut_assert(mytable.length() == size - i - 1, "Failed to get size\n");
    }

    /* Check Empty */
    ut_assert(mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
    ut_assert(mytable.length() == 0, "Failed to remove all entries\n");

    return failures == 0 ? 0 : -1;
}

/*--------------------------------------------------------------------------------------
 * testStress 
 *--------------------------------------------------------------------------------------*/
int UT_Table::testStress(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    int key, data;
    int size = 64;
    int data_order[64];
    int test_cycles = 65536;
    int key_range = 0xFFFFFFFF;
    int num_added = 0;
    Table<int,int> mytable(size);

    failures = 0;

    /* Seed Random Number */
    srand((unsigned int)TimeLib::latchtime());

    /* Cycle Tests */
    for(int j = 0; j < test_cycles; j++)
    {
        /* Reset Test */
        num_added = 0;

        /* Load Hash */
        for(int i = 0; i < size; i++)
        {
            key = rand() % key_range;
            if(mytable.add(key, key, false))
            {
                data_order[num_added++] = key;
            }
        }

        /* Check Hash */
        for(int i = 0; i < num_added; i++)
        {
            key = data_order[i];

            mytable.first(&data);
            ut_assert(data == key, "Failed to get next key %d != %d\n", data, key);
            mytable.first(&data);
            ut_assert(data == key, "Failed to get same key %d != %d\n", data, key);
            ut_assert(mytable.remove(key), "Failed to remove key %d\n", key);
        }

        /* Check Empty */
        ut_assert(mytable.first(&data) == (int)INVALID_KEY, "Failed to get error\n");
        ut_assert(mytable.length() == 0, "Failed to remove all entries\n");
    }

    return failures == 0 ? 0 : -1;
}
