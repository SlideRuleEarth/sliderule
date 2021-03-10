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
    const char* logfile = NULL;
    if(argc > 0) logfile = StringLib::checkNullStr(argv[0]);
    return new UT_Dictionary(cmd_proc, name, logfile);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_Dictionary::UT_Dictionary(CommandProcessor* cmd_proc, const char* obj_name, const char* logfile):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    if(logfile)
    {
        if(StringLib::match(logfile, "STDOUT"))
        {
            testlog = stdout;
        }
        else
        {
            testlog = fopen(logfile, "w");
        }
    }
    else
    {
        #ifdef _LINUX_
            testlog = fopen("/dev/null", "w");
        #else
        #ifdef _WINDOWS_
            testlog = fopen("NUL", "w");
        #endif
        #endif
    }

    /* Register Commands */
    registerCommand("FUNCTIONAL_TEST", (cmdFunc_t)&UT_Dictionary::functionalUnitTestCmd, 1, "<set name>");
    registerCommand("ITERATOR_TEST", (cmdFunc_t)&UT_Dictionary::iteratorUnitTestCmd, 1, "<set name>");
    registerCommand("ADD_WORD_SET", (cmdFunc_t)&UT_Dictionary::addWordSetCmd, 2, "<set name> <filename>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Dictionary::~UT_Dictionary(void)
{
}

/*----------------------------------------------------------------------------
 * functionalUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::functionalUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    Dictionary<long> d1;

    long seq;
    int hash_size;
    int max_chain;
    int num_entries;

    bool failure=false;

    /* Start Timer */
    int64_t start_time = TimeLib::gettimems();

    /* Get Word List */
    const char* wordset_name = argv[0];
    List<SafeString*>* wordlist_ptr;
    try
    {
        wordlist_ptr = (List<SafeString*>*)wordsets[wordset_name];
        if(wordlist_ptr->length() <= 0)
        {
            mlog(RAW, "[%d] ERROR: word set %s is empty!\n", __LINE__, wordset_name);
            return -1;
        }
    }
    catch(std::out_of_range& e)
    {
        mlog(RAW, "[%d] ERROR: unable to locate word set %s: %s\n", __LINE__, wordset_name, e.what());
        return -1;
    }

    /* Get Number of Words */
    List<SafeString*>& wordset = *wordlist_ptr;
    int numwords = wordset.length();

    /* Set Entries */
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        if(!d1.add(wordset[i]->getString(), seq))
        {
            mlog(RAW, "[%d] ERROR: failed to add %s\n", __LINE__, wordset[i]->getString());
            failure = true;
        }
        else
        {
            fprintf(testlog, "Added entry: (%s, %ld) --> %d\n", wordset[i]->getString(), seq, d1.length());
        }
    }

    /* Find Entries */
    for(int i = 0; i < numwords; i++)
    {
        if(!d1.find(wordset[i]->getString()))
        {
            mlog(RAW, "[%d] ERROR: failed to find %s\n", __LINE__, wordset[i]->getString());
            failure = true;
        }
        else
        {
            fprintf(testlog, "Found entry: (%s)\n", wordset[i]->getString());
        }
    }

    /* Get Entries */
    for(int i = 0; i < numwords; i++)
    {
        try
        {
            long data = d1.get(wordset[i]->getString());
            if(data != i)
            {
                mlog(RAW, "[%d] ERROR: failed to read back value, %ld != %d, for word: %s\n", __LINE__, data, i, wordset[i]->getString());
                failure = true;
            }
            else
            {
                fprintf(testlog, "Got entry: (%s, %ld)\n", wordset[i]->getString(), data);
            }
        }
        catch(std::out_of_range& e)
        {
            mlog(RAW, "[%d] ERROR: failed to get %s: %s\n", __LINE__, wordset[i]->getString(), e.what());
            failure = true;
        }
    }

    /* Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    mlog(INFO, "Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != numwords)
    {
        mlog(RAW, "[%d] ERROR: incorrect number of entries %d != %d\n", __LINE__, num_entries, numwords);
        failure = true;
    }

    /* Get Keys */
    if(numwords < 10000)
    {
        char** key_list = NULL;
        int num_keys = d1.getKeys(&key_list);
        if(num_keys != numwords)
        {
            mlog(RAW, "[%d] ERROR: retrieved the wrong number of keys %d != %d\n", __LINE__, num_keys, numwords);
            failure = true;
        }

        const char** true_list = new const char* [numwords];
        for(int i = 0; i < numwords; i++)
        {
            true_list[i] = wordset[i]->getString();
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
                        if(StringLib::match(true_list[j], key_list[i]))
                        {
                            fprintf(testlog, "Found key: (%s)\n", key_list[i]);
                            found = true;
                            true_list[j] = NULL;
                            break;
                        }
                    }
                }
                if(!found)
                {
                    mlog(RAW, "[%d] ERROR: failed to retrieve the correct key, %s\n", __LINE__, wordset[i]->getString());
                    failure = true;
                }

                delete [] key_list[i];
            }
        }
        if(key_list) delete [] key_list;
        delete [] true_list;
    }

    /* Remove Entries */
    for(int i = 0; i < numwords; i++)
    {
        if(d1.remove(wordset[i]->getString()) != true)
        {
            mlog(RAW, "[%d] ERROR: failed to remove %s, %d\n", __LINE__, wordset[i]->getString(), i);
            failure = true;
        }
        else
        {
            fprintf(testlog, "Removed entry: (%s)\n", wordset[i]->getString());
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    mlog(INFO, "Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        mlog(RAW, "[%d] ERROR: incorrect number of entries %d != 0\n", __LINE__, num_entries);
        failure = true;
    }

    /* Set Entries */
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        if(!d1.add(wordset[i]->getString(), seq))
        {
            mlog(RAW, "[%d] ERROR: failed to add %s\n", __LINE__, wordset[i]->getString());
            failure = true;
        }
        else
        {
            fprintf(testlog, "Re-added entry: (%s, %ld) --> %d\n", wordset[i]->getString(), seq, d1.length());
        }
    }

    /* Clear Entries */
    d1.clear();

    /* Find Entries - Should Not Find Them */
    for(int i = 0; i < numwords; i++)
    {
        if(d1.find(wordset[i]->getString()))
        {
            mlog(RAW, "[%d] ERROR: found entry that should have been cleared %s\n", __LINE__, wordset[i]->getString());
            failure = true;
        }
        else
        {
            fprintf(testlog, "Correctly did not find entry: (%s)\n", wordset[i]->getString());
        }
    }

    /* Re-Check Attributes */
    hash_size = d1.getHashSize();
    max_chain = d1.getMaxChain();
    num_entries = d1.length();
    mlog(INFO, "Hash Size, Max Chain, Num Entries, %d, %d, %d\n", hash_size, max_chain,  num_entries);
    if(num_entries != 0)
    {
        mlog(RAW, "[%d] ERROR: incorrect number of entries %d != 0\n", __LINE__, num_entries);
        failure = true;
    }

    /* Start Timer */
    int64_t stop_time = TimeLib::gettimems();
    double elapsed_time = (double)(stop_time - start_time) / 1000.0;
    mlog(INFO, "Time to complete: %lf seconds\n", elapsed_time);

    /* Return Status */
    if(failure) return -1;
    else        return 0;
}

/*----------------------------------------------------------------------------
 * iteratorUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::iteratorUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    Dictionary<long> d1;
    long seq, sum;
    bool failure=false;

    /* Get Word List */
    const char* wordset_name = argv[0];
    List<SafeString*>* wordlist_ptr;
    try
    {
        wordlist_ptr = (List<SafeString*>*)wordsets[wordset_name];
        if(wordlist_ptr->length() <= 0)
        {
            mlog(RAW, "[%d] ERROR: word set %s is empty!\n", __LINE__, wordset_name);
            return -1;
        }
    }
    catch(std::out_of_range& e)
    {
        mlog(RAW, "[%d] ERROR: unable to locate word set %s: %s\n", __LINE__, wordset_name, e.what());
        return -1;
    }

    /* Get Word Set */
    List<SafeString*>& wordset = *wordlist_ptr;
    int numwords = wordset.length();

    /* Set Entries */
    sum = 0;
    for(int i = 0; i < numwords; i++)
    {
        seq = i;
        sum += i;
        if(!d1.add(wordset[i]->getString(), seq))
        {
            mlog(RAW, "[%d] ERROR: failed to add %s\n", __LINE__, wordset[i]->getString());
            failure = true;
        }
        else
        {
            fprintf(testlog, "Added entry: (%s, %ld) --> %d\n", wordset[i]->getString(), seq, d1.length());
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
        mlog(RAW, "[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
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
        mlog(RAW, "[%d] ERROR: the values did not correctly sum, %ld != %ld\n", __LINE__, tsum, sum);
        failure = true;
    }

    /* Return Status */
    if(failure) return -1;
    else        return 0;
}

/*----------------------------------------------------------------------------
 * addWordSetCmd  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::addWordSetCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* setname = argv[0];
    const char* filename = argv[1];

    int numwords = createWordSet(setname, filename);

    if(numwords > 0)    return 0;
    else                return -1;
}

/*----------------------------------------------------------------------------
 * createWordSet  -
 *----------------------------------------------------------------------------*/
int UT_Dictionary::createWordSet (const char* name, const char* filename)
{
    List<SafeString*>* wordlist = new List<SafeString*>();

    FILE* wordfile = fopen(filename, "r");
    if(wordfile == NULL)
    {
       mlog(RAW, "[%d] ERROR: Unable to open word list file: %s\n", __LINE__, filename);
       delete wordlist;
       return -1;
    }

    while(true)
    {
        char line[MAX_STR_SIZE];
        if(StringLib::getLine(line, NULL, MAX_STR_SIZE, wordfile) == 0)
        {
            if((line[0] != '\n') && line[0] != '\0')
            {
                SafeString* word = new SafeString("%s", line);
                wordlist->add(word);
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
        return wordlist->length();
    }
    else
    {
        mlog(CRITICAL, "Failed to add word list %s, possibly duplicate name exists\n", name);
        delete wordlist;
        return -1;
    }
}
