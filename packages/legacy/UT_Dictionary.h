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

#ifndef __ut_dictionary__
#define __ut_dictionary__

#include "List.h"
#include "StringLib.h"
#include "Dictionary.h"
#include "CommandableObject.h"
#include "OsApi.h"

/******************************************************************************
 * UNIT TEST DICTIONARY CLASS
 ******************************************************************************/

class UT_Dictionary: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<List<SafeString*>*> wordsets;
        FILE* testlog;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        UT_Dictionary (CommandProcessor* cmd_proc, const char* obj_name, const char* logfile);
        ~UT_Dictionary (void);

        int functionalUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int iteratorUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int addWordSetCmd (int argc, char argv[][MAX_CMD_SIZE]);

        int createWordSet (const char* name, const char* filename);
        static void freeWordSet (void* obj, void* parm);
};

#endif  /* __ut_dictionary__ */
