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

#ifndef __osapi__
#define __osapi__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <assert.h>

/******************************************************************************
 * MACROS
 ******************************************************************************/

#ifndef NULL
#define NULL        ((void *) 0)
#endif

#ifndef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

#ifndef CompileTimeAssert
#define CompileTimeAssert(Condition, Message) typedef char Message[(Condition) ? 1 : -1]
#endif

#ifdef _GNU_
#define VARG_CHECK(f, a, b) __attribute__((format(f, a, b)))
#else
#define VARG_CHECK(f, a, b)
#endif

#ifdef __BE__
#define NATIVE_FLAGS 1
#else
#define NATIVE_FLAGS 0
#endif

#define PATH_DELIMETER '/'
#define PATH_DELIMETER_STR "/"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* Ordered Key */
typedef unsigned long okey_t;

/* File */
typedef FILE* fileptr_t;

/* Check Sizes */
CompileTimeAssert(sizeof(bool)==1, TypeboolWrongSize);

/******************************************************************************
 * DEFINES
 ******************************************************************************/

/* Return Codes */
#define TIMEOUT_RC                  (0)
#define INVALID_RC                  (-1)
#define SHUTDOWN_RC                 (-2)
#define TCP_ERR_RC                  (-3)
#define UDP_ERR_RC                  (-4)
#define SOCK_ERR_RC                 (-5)
#define BUFF_ERR_RC                 (-6)
#define WOULDBLOCK_RC               (-7)
#define PARM_ERR_RC                 (-8)
#define TTY_ERR_RC                  (-9)
#define ACC_ERR_RC                  (-10)

/* I/O Definitions */
#define IO_PEND                     (-1)
#define IO_CHECK                    (0)
#define IO_DEFAULT_TIMEOUT          (1000) // ms
#define IO_DEFAULT_MAXSIZE          (0x10000) // bytes
#define IO_INFINITE_CONNECTIONS     (-1)
#define IO_READ_FLAG                (POLLIN)
#define IO_WRITE_FLAG               (POLLOUT)
#define IO_ALIVE_FLAG               (0x100)
#define IO_CONNECT_FLAG             (0x200)
#define IO_DISCONNECT_FLAG          (0x400)
#define SYS_TIMEOUT                 (LocalLib::getIOTimeout()) // ms
#define SYS_MAXSIZE                 (LocalLib::getIOMaxsize()) // bytes

/* Ordered Keys */
#define INVALID_KEY                 0xFFFFFFFFFFFFFFFFLL

/* Debug Logging */
#define dlog(...)                   LocalLib::print(__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Thread.h"
#include "Mutex.h"
#include "Cond.h"
#include "Sem.h"
#include "RTExcept.h"
#include "Timer.h"
#include "LocalLib.h"
#include "SockLib.h"
#include "TTYLib.h"

#endif  /* __osapi__ */
