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

#include "OsApi.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Thread::Thread(thread_func_t function, void* parm, bool _join)
{
    join = _join;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    if(!join) pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&threadId, &pthread_attr, function, parm);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Thread::~Thread()
{
    if(join)
    {
        int ret = pthread_join(threadId, NULL);
        if(ret != 0) dlog("Failed to join thread (%d): %s\n", ret, strerror(ret));
    }
}
