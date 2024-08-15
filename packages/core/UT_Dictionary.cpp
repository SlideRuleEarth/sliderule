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
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Dictionary::TYPE = "UT_Dictionary";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_Dictionary::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    return new UT_Dictionary(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_Dictionary::UT_Dictionary(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("FUNCTIONAL_TEST", reinterpret_cast<cmdFunc_t>(&UT_Dictionary::functionalUnitTestCmd), 1, "<set name>");
    registerCommand("ITERATOR_TEST", reinterpret_cast<cmdFunc_t>(&UT_Dictionary::iteratorUnitTestCmd), 1, "<set name>");
    registerCommand("ADD_WORD_SET", reinterpret_cast<cmdFunc_t>(&UT_Dictionary::addWordSetCmd), 3, "<set name> <filename> <num words in set>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Dictionary::~UT_Dictionary(void) = default;

/*----------------------------------------------------------------------------
 * functionalUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::functionalUnitTestCmd (int argc, const char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    Dictionary<long> d1;

    long seq;
    int hash_size;
    int max_chain;
    int num_entries;

    bool failure=false;

    /* Start Timer */
    const int64_t start_time = TimeLib::gpstime();

    /* Get Word List */
    const char* wordset_name = argv[0];
    vector<string>* wordlist_ptr;
    try
    {
        wordlist_ptr = wordsets[wordset_name];
        if(wordlist_ptr->empty())
        {
            print2term("[%d] ERROR: word set %s is empty!\n", __LINE__, wordset_name);
            return -1;
        }
    }
    catch(RunTimeException& e)
    {
        print2term("[%d] ERROR: unable to locate word set %s: %s\n", __LINE__, wordset_name, e.what());
        return -1;
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
            print2term("[%d] ERROR: failed to add %s\n", __LINE__, wordset[i].c_str());
            failure = true;
        }
    }

    /* Find Entries */
    for(int i = 0; i < numwords; i++)
    {
        if(!d1.find(wordset[i].c_str()))
        {
            print2term("[%d] ERROR: failed to find %s\n", __LINE__, wordset[i].c_str());
            failure = true;
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
                print2term("[%d] ERROR: failed to read back value, %ld != %d, for word: %s\n", __LINE__, data, i, wordset[i].c_str());
                failure = true;
            }
        }
        catch(RunTimeException& e)
        {
            print2term("[%d] ERROR: failed to get %s: %s\n", __LINE__, wordset[i].c_str(), e.what());
            failure = true;
        }
    }

    /* Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != numwords)
    {
        print2term("[%d] ERROR: incorrect number of entries %d != %d\n", __LINE__, num_entries, numwords);
        failure = true;
    }

    /* Get Keys */
    if(numwords < 10000)
    {
        char** key_list = NULL;
        const int num_keys = d1.getKeys(&key_list);
        if(num_keys != numwords)
        {
            print2term("[%d] ERROR: retrieved the wrong number of keys %d != %d\n", __LINE__, num_keys, numwords);
            failure = true;
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
                    print2term("[%d] ERROR: failed to retrieve the correct key, %s\n", __LINE__, wordset[i].c_str());
                    failure = true;
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
            print2term("[%d] ERROR: failed to remove %s, %d\n", __LINE__, wordset[i].c_str(), i);
            failure = true;
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        print2term("[%d] ERROR: incorrect number of entries %d != 0\n", __LINE__, num_entries);
        failure = true;
    }

    /* Set Entries */
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        if(!d1.add(wordset[i].c_str(), seq))
        {
            print2term("[%d] ERROR: failed to add %s\n", __LINE__, wordset[i].c_str());
            failure = true;
        }
    }

    /* Clear Entries */
    d1.clear();

    /* Find Entries - Should Not Find Them */
    for(int i = 0; i < numwords; i++)
    {
        if(d1.find(wordset[i].c_str()))
        {
            print2term("[%d] ERROR: found entry that should have been cleared %s\n", __LINE__, wordset[i].c_str());
            failure = true;
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    print2term("Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        print2term("[%d] ERROR: incorrect number of entries %d != 0\n", __LINE__, num_entries);
        failure = true;
    }

    /* Start Timer */
    const int64_t stop_time = TimeLib::gpstime();
    const double elapsed_time = (double)(stop_time - start_time) / 1000.0;
    print2term("Time to complete: %lf seconds\n", elapsed_time);

    /* Return Status */
    if(failure) return -1;
    else        return 0;
}

/*----------------------------------------------------------------------------
 * iteratorUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::iteratorUnitTestCmd (int argc, const char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    Dictionary<long> d1;
    bool failure=false;

    /* Get Word List */
    const char* wordset_name = argv[0];
    vector<string>* wordlist_ptr;
    try
    {
        wordlist_ptr = wordsets[wordset_name];
        if(wordlist_ptr->empty())
        {
            print2term("[%d] ERROR: word set %s is empty!\n", __LINE__, wordset_name);
            return -1;
        }
    }
    catch(RunTimeException& e)
    {
        print2term("[%d] ERROR: unable to locate word set %s: %s\n", __LINE__, wordset_name, e.what());
        return -1;
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
            print2term("[%d] ERROR: failed to add %s\n", __LINE__, wordset[i].c_str());
            failure = true;
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
        print2term("[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
        failure = true;
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
        print2term("[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
        failure = true;
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
        print2term("[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
        failure = true;
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
        print2term("[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
        failure = true;
    }

    /* Return Status */
    if(failure) return -1;
    else        return 0;
}

/*----------------------------------------------------------------------------
 * addWordSetCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::addWordSetCmd (int argc, const char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* setname = argv[0];
    const char* filename = argv[1];
    const char* size_str = argv[2];

    long size = 0;
    if(!StringLib::str2long(size_str, &size)) return -1;

    const int numwords = createWordSet(setname, filename);

    if(numwords == size)    return 0;
    else                    return -1;
}

/*----------------------------------------------------------------------------
 * createWordSet  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::createWordSet (const char* name, const char* filename)
{
    FILE* wordfile = fopen(filename, "r");
    if(wordfile == NULL)
    {
       print2term("[%d] ERROR: Unable to open word list file: %s\n", __LINE__, filename);
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
