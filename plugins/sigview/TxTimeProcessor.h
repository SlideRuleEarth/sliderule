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

#ifndef __tx_time_processor__
#define __tx_time_processor__

#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class TxTimeProcessor: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* obj_name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            active;
        int             timesPointer;
        long            lastDelta;
        Subscriber*     txtimeq;
        int             pce;
        long*           absoluteTimes;
        pthread_t       pid;
        /*--------------------------------------------------------------------

         * Methods
         *--------------------------------------------------------------------*/

                        TxTimeProcessor     (CommandProcessor* cmd_proc, const char* obj_name, int pcenum, const char* txtimeq_name);
                        ~TxTimeProcessor    (void);

    static   void*      txTimeThread        (void*);
             void       checkDelta          (long time);

};

#endif  /* __tx_time_processor__ */
