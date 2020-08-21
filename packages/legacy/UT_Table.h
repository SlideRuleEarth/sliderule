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

#ifndef __ut_table__
#define __ut_table__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "core.h"

/******************************************************************************
 * UNIT TEST TIME LIBRARY CLASS
 ******************************************************************************/

class UT_Table: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;
        static const int UT_MAX_ASSERT = 256;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int failures;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

            UT_Table            (CommandProcessor* cmd_proc, const char* obj_name);
            ~UT_Table           (void);

    bool    _ut_assert          (bool e, const char* file, int line, const char* fmt, ...);

	int     testAddRemove       (int argc, char argv[][MAX_CMD_SIZE]);
	int     testChaining        (int argc, char argv[][MAX_CMD_SIZE]);
	int     testRemoving        (int argc, char argv[][MAX_CMD_SIZE]);
	int     testDuplicates      (int argc, char argv[][MAX_CMD_SIZE]);
	int     testFullTable       (int argc, char argv[][MAX_CMD_SIZE]);
	int     testCollisions      (int argc, char argv[][MAX_CMD_SIZE]);
	int     testStress          (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __ut_table__ */
