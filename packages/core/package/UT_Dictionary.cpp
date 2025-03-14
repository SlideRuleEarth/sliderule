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

#include "UT_Dictionary.h"
#include "UnitTest.h"
#include "OsApi.h"
#include "TimeLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Dictionary::LUA_META_NAME = "UT_Dictionary";
const struct luaL_Reg UT_Dictionary::LUA_META_TABLE[] = {
    {"functional",  functionalUnitTestCmd},
    {"iterator",    iteratorUnitTestCmd},
    {"add_wordset", addWordSetCmd},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_Dictionary(L));
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
UT_Dictionary::UT_Dictionary (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * functionalUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::functionalUnitTestCmd (lua_State* L)
{
    UT_Dictionary* lua_obj = NULL;
    const char* wordset_name = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Dictionary*>(getLuaSelf(L, 1));
        wordset_name = getLuaString(L, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }
    
    Dictionary<long> d1;

    long seq;
    int hash_size;
    int max_chain;
    int num_entries;

    ut_initialize(lua_obj);

    /* Start Timer */
    const int64_t start_time = TimeLib::gpstime();

    /* Get Word List */
    vector<string>* wordlist_ptr;
    try
    {
        wordlist_ptr = lua_obj->wordsets[wordset_name];
        if(wordlist_ptr->empty())
        {
            ut_assert(lua_obj, false, "ERROR: word set %s is empty!\n", wordset_name);
            lua_pushboolean(L, ut_status(lua_obj));
            return 1;
        }
    }
    catch(RunTimeException& e)
    {
        ut_assert(lua_obj, false, "ERROR: unable to locate word set %s: %s\n", wordset_name, e.what());
        lua_pushboolean(L, ut_status(lua_obj));
        return 1;
    }

    /* Get Number of Words */
    vector<string>& wordset = *wordlist_ptr;
    const int numwords = static_cast<int>(wordset.size());

    /* Set Entries */
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        if(!d1.add(wordset[i].c_str(), seq))
        {
            ut_assert(lua_obj, false, "ERROR: failed to add %s\n", wordset[i].c_str());
        }
    }

    /* Find Entries */
    for(int i = 0; i < numwords; i++)
    {
        if(!d1.find(wordset[i].c_str()))
        {
            ut_assert(lua_obj, false, "ERROR: failed to find %s\n", wordset[i].c_str());
        }
    }

    /* Get Entries */
    for(int i = 0; i < numwords; i++)
    {
        try
        {
            const long data = d1.get(wordset[i].c_str());
            if(data != i)
            {
                ut_assert(lua_obj, false, "ERROR: failed to read back value, %ld != %d, for word: %s\n", data, i, wordset[i].c_str());
            }
        }
        catch(RunTimeException& e)
        {
            ut_assert(lua_obj, false, "ERROR: failed to get %s: %s\n", wordset[i].c_str(), e.what());
        }
    }

    /* Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != numwords)
    {
        ut_assert(lua_obj, false, "ERROR: incorrect number of entries %d != %d\n", num_entries, numwords);
    }

    /* Get Keys */
    if(numwords < 10000)
    {
        char** key_list = NULL;
        const int num_keys = d1.getKeys(&key_list);
        if(num_keys != numwords)
        {
            ut_assert(lua_obj, false, "ERROR: retrieved the wrong number of keys %d != %d\n", num_keys, numwords);
        }

        const char** true_list = new const char* [numwords];
        for(int i = 0; i < numwords; i++)
        {
            true_list[i] = wordset[i].c_str();
        }

        for(int i = 0; i < numwords; i++)
        {
            if(key_list && i < num_keys)
            {
                bool found = false;
                for(int j = 0; j < numwords; j++)
                {
                    if(true_list[j] != NULL)
                    {
                        if(StringLib::match(true_list[j], key_list[i])) // NOLINT(clang-analyzer-core.CallAndMessage)
                        {
                            found = true;
                            true_list[j] = NULL;
                            break;
                        }
                    }
                }
                if(!found)
                {
                    ut_assert(lua_obj, false, "ERROR: failed to retrieve the correct key, %s\n", wordset[i].c_str());
                }

                delete [] key_list[i]; // NOLINT(clang-analyzer-core.CallAndMessage)
            }
        }
        delete [] key_list;
        delete [] true_list;
    }

    /* Remove Entries */
    for(int i = 0; i < numwords; i++)
    {
        if(d1.remove(wordset[i].c_str()) != true)
        {
            ut_assert(lua_obj, false, "ERROR: failed to remove %s, %d\n", wordset[i].c_str(), i);
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        ut_assert(lua_obj, false, "ERROR: incorrect number of entries %d != 0\n", num_entries);
    }

    /* Set Entries */
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        if(!d1.add(wordset[i].c_str(), seq))
        {
            ut_assert(lua_obj, false, "ERROR: failed to add %s\n", wordset[i].c_str());
        }
    }

    /* Clear Entries */
    d1.clear();

    /* Find Entries - Should Not Find Them */
    for(int i = 0; i < numwords; i++)
    {
        if(d1.find(wordset[i].c_str()))
        {
            ut_assert(lua_obj, false, "ERROR: found entry that should have been cleared %s\n", wordset[i].c_str());
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        ut_assert(lua_obj, false, "ERROR: incorrect number of entries %d != 0\n", num_entries);
    }

    /* Start Timer */
    const int64_t stop_time = TimeLib::gpstime();
    const double elapsed_time = (double)(stop_time - start_time) / 1000.0;
    print2term("Time to complete: %lf seconds\n", elapsed_time);

    /* Return Status */
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * iteratorUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::iteratorUnitTestCmd (lua_State* L)
{
    UT_Dictionary* lua_obj = NULL;
    const char* wordset_name = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Dictionary*>(getLuaSelf(L, 1));
        wordset_name = getLuaString(L, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    Dictionary<long> d1;

    ut_initialize(lua_obj);

    /* Get Word List */
    vector<string>* wordlist_ptr;
    try
    {
        wordlist_ptr = lua_obj->wordsets[wordset_name];
        if(wordlist_ptr->empty())
        {
            ut_assert(lua_obj, false, "ERROR: word set %s is empty!\n", wordset_name);
            lua_pushboolean(L, ut_status(lua_obj));
            return 1;
        }
    }
    catch(RunTimeException& e)
    {
        ut_assert(lua_obj, false, "ERROR: unable to locate word set %s: %s\n", wordset_name, e.what());
        lua_pushboolean(L, ut_status(lua_obj));
        return 1;
    }

    /* Get Word Set */
    vector<string>& wordset = *wordlist_ptr;
    const int numwords = static_cast<int>(wordset.size());

    /* Set Entries */
    long sum = 0;
    for(int i = 0; i < numwords; i++)
    {
        const long seq = i;
        sum += i;
        if(!d1.add(wordset[i].c_str(), seq))
        {
            ut_assert(lua_obj, false, "ERROR: failed to add %s\n", wordset[i].c_str());
        }
    }

    /* Iteration Test Variables */
    long val;
    long tsum;

    /* Iterate Forward Through Dictionary */
    tsum = 0;
    {
        const char* word_key = d1.first(&val);
        while(word_key)
        {
            tsum += val;
            word_key = d1.next(&val);
        }
    }

    /* Check Forward Iteration Results */
    if(tsum != sum)
    {
        ut_assert(lua_obj, false, "ERROR: the values did not correctly sum, %ld != %ld\n", tsum, sum);
    }

    /* Iterate Backwards Through Dictionary */
    tsum = 0;
    {
        const char* word_key = d1.last(&val);
        while(word_key)
        {
            tsum += val;
            word_key = d1.prev(&val);
        }
    }

    /* Check Backward Iteration Results */
    if(tsum != sum)
    {
        ut_assert(lua_obj, false, "ERROR: the values did not correctly sum, %ld != %ld\n", tsum, sum);
    }

    /* Iterate via Iterator Through Dictionary */
    tsum = 0;
    {
        Dictionary<long>::Iterator iterator(d1);
        for(int i = 0; i < iterator.length; i++)
        {
            tsum += iterator[i].value;
        }
    }

    /* Check Iterator Iteration Results */
    if(tsum != sum)
    {
        ut_assert(lua_obj, false, "ERROR: the values did not correctly sum, %ld != %ld\n", tsum, sum);
    }

    /* Iterate via Iterator Backward Through Dictionary */
    tsum = 0;
    {
        Dictionary<long>::Iterator iterator(d1);
        for(int i = iterator.length - 1; i >= 0 ; i--)
        {
            tsum += iterator[i].value;
        }
    }

    /* Check Backward Iterator Iteration Results */
    if(tsum != sum)
    {
        ut_assert(lua_obj, false, "ERROR: the values did not correctly sum, %ld != %ld\n", tsum, sum);
    }

    /* Return Status */
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * addWordSetCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::addWordSetCmd (lua_State* L)
{
    UT_Dictionary* lua_obj = NULL;
    const char* setname = NULL;
    const char* filename = NULL;
    long size = 0;

    try
    {
        lua_obj = dynamic_cast<UT_Dictionary*>(getLuaSelf(L, 1));
        setname = getLuaString(L, 2);
        filename = getLuaString(L, 3);
        size = getLuaInteger(L, 4);        
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);
    const int numwords = lua_obj->createWordSet(setname, filename);
    ut_assert(lua_obj, numwords == size, "Incorrect number of words: %d != %d", numwords, size);

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * createWordSet  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::createWordSet (const char* name, const char* filename)
{
    FILE* wordfile = fopen(filename, "r");
    if(wordfile == NULL)
    {
       mlog(CRITICAL, "ERROR: Unable to open word list file: %s\n", filename);
       return -1;
    }

    vector<string>* wordlist = new vector<string>;

    while(true)
    {
        char line[MAX_STR_SIZE];
        if(StringLib::getLine(line, NULL, MAX_STR_SIZE, wordfile) == 0)
        {
            if((line[0] != '\n') && line[0] != '\0')
            {
                const string word(line);
                wordlist->push_back(word);
            }
        }
        else
        {
            break;
        }
    }

    fclose(wordfile);

    if(wordsets.add(name, wordlist, true))
    {
        return wordlist->size();
    }
    else
    {
        mlog(CRITICAL, "Failed to add word list %s, possibly duplicate name exists\n", name);
        delete wordlist;
        return -1;
    }
}
