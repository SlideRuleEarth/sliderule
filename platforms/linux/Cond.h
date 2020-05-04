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

#ifndef __condition__
#define __condition__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Mutex.h"

#include <pthread.h>

/******************************************************************************
 * CONDITION CLASS
 ******************************************************************************/

class Cond: public Mutex
{
    public:

        typedef enum {
            NOTIFY_ONE,
            NOTIFY_ALL
        } notify_t;
        
        Cond (int num_sigs=1);
        ~Cond (void);

        void signal (int sig=0, notify_t notify=NOTIFY_ALL);
        bool wait (int sig=0, int timeout_ms=IO_PEND);

    private:

        int numSigs;
        pthread_cond_t* condId;
};

#endif  /* __condition__ */
