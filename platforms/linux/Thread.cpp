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

#include "OsApi.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

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

/*----------------------------------------------------------------------------
 * getId
 *----------------------------------------------------------------------------*/
long Thread::getId(void)
{
    pid_t pid = gettid();
    return (long)pid;
}

/*----------------------------------------------------------------------------
 * createGlobal
 *----------------------------------------------------------------------------*/
Thread::key_t Thread::createGlobal (void)
{
    pthread_key_t key;
    pthread_key_create(&key, NULL);
    return (key_t)key;
}

/*----------------------------------------------------------------------------
 * setGlobal
 *----------------------------------------------------------------------------*/
int Thread::setGlobal (key_t key, void* value)
{
    return pthread_setspecific((pthread_key_t)key, value); 
}

/*----------------------------------------------------------------------------
 * getGlobal
 *----------------------------------------------------------------------------*/
void* Thread::getGlobal (key_t key)
{
    return pthread_getspecific((pthread_key_t)key);
}
