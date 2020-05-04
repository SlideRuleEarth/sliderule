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

#ifndef __ut_timelib__
#define __ut_timelib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "core.h"

/******************************************************************************
 * UNIT TEST TIME LIBRARY CLASS
 ******************************************************************************/

class UT_TimeLib: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*      TYPE;
        static const int64_t    Truth_Times[39][2];
        TimeLib::gmt_time_t     Truth_GMT[39];
        static const int        UNIX_Epoch_Start;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

            UT_TimeLib          (CommandProcessor* cmd_proc, const char* obj_name);
            ~UT_TimeLib         (void);
	int     CheckGmt2GpsCmd     (int argc, char argv[][MAX_CMD_SIZE]);
	int     CheckGps2GmtCmd     (int argc, char argv[][MAX_CMD_SIZE]);
	int     CheckGetCountCmd    (int argc, char argv[][MAX_CMD_SIZE]);
	void    initTruthGMT        (void);
};

#endif  /* __ut_timelib__ */
