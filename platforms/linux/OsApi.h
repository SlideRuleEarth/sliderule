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
#include <stddef.h>
#include <string.h>
#include <poll.h>
#include <assert.h>

/******************************************************************************
 * PLATFORM DEFINES
 ******************************************************************************/

#include "platform.h"

#ifndef LIBID
#define LIBID "local"
#endif

#ifndef CONFDIR
#define CONFDIR "."
#endif

#ifndef PLUGINDIR
#define PLUGINDIR "."
#endif

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

#ifdef __GNUC__
#define VARG_CHECK(f, a, b) __attribute__((format(f, a, b)))
#else
#define VARG_CHECK(f, a, b)
#endif

#ifdef __be__
#define NATIVE_FLAGS 1
#else
#define NATIVE_FLAGS 0
#endif

#define ALIGN_PACKED  __attribute__((packed))

#define PATH_DELIMETER '/'
#define PATH_DELIMETER_STR "/"

#define ORIGIN  0

#ifndef MAX_STR_SIZE
#define MAX_STR_SIZE 1024
#endif

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* Event Levels */
typedef enum {
    DEBUG               = 0,
    INFO                = 1,
    WARNING             = 2,
    ERROR               = 3,
    CRITICAL            = 4,
    INVALID_EVENT_LEVEL = 5
} event_level_t;

/* Exceptions */
typedef enum {
    RTE_INFO                    =  0,
    RTE_ERROR                   = -1,
    RTE_TIMEOUT                 = -2,
    RTE_RESOURCE_DOES_NOT_EXIST = -3,
    RTE_EMPTY_SUBSET            = -4,
    RTE_SIMPLIFY                = -5
} rte_t;

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
#define SYS_TIMEOUT                 (OsApi::getIOTimeout()) // ms
#define SYS_MAXSIZE                 (OsApi::getIOMaxsize()) // bytes

/* Ordered Keys */
#define INVALID_KEY                 0xFFFFFFFFFFFFFFFFLL

/* Debug Logging */
#define dlog(...)                   OsApi::print(__FILE__,__LINE__,__VA_ARGS__)
#ifdef __terminal__
#define print2term(...)             printf(__VA_ARGS__)
#else
#define print2term(...)             do { if (0) printf(__VA_ARGS__); } while (0)
#endif

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RunTimeException.h"
#include "Thread.h"
#include "Mutex.h"
#include "Cond.h"
#include "Sem.h"
#include "Timer.h"
#include "SockLib.h"
#include "TTYLib.h"

/******************************************************************************
 * Standard Template Classes
 ******************************************************************************/

#include <memory>
#include <vector>
#include <string>

using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::vector;
using std::string;

/******************************************************************************
 * LIBRARY CLASS
 ******************************************************************************/

class OsApi
{
    public:

        typedef void (*print_func_t) (const char* file_name, unsigned int line_number, const char* message);

        static const int MAX_PRINT_MESSAGE = 256;
        static const int SYS_CLK = 0; // system clock that can be converted into civil time
        static const int CPU_CLK = 1; // processor clock that only counts ticks

        static void         init                (print_func_t _print_func);
        static void         deinit              (void);

        static void         sleep               (double secs); // sleep at highest resolution available on system
        static void         dupstr              (const char** dst, const char* src);
        static int64_t      time                (int clkid);
        static int64_t      timeres             (int clkid); // returns the resolution of the clock
        static uint16_t     swaps               (uint16_t val);
        static uint32_t     swapl               (uint32_t val);
        static uint64_t     swapll              (uint64_t val);
        static float        swapf               (float val);
        static double       swaplf              (double val);
        static int          nproc               (void);
        static double       memusage            (void);
        static void         print               (const char* file_name, unsigned int line_number, const char* format_string, ...)  __attribute__((format(printf, 3, 4)));

        static bool         setIOMaxsize        (int maxsize);
        static int          getIOMaxsize        (void);
        static void         setIOTimeout        (int timeout);
        static int          getIOTimeout        (void);
        static int          performIOTimeout    (void);
        static int64_t      getLaunchTime       (void);
        static void         setEnvVersion       (const char* verstr);
        static const char*  getEnvVersion       (void);
        static void         setIsPublic         (bool _is_public);
        static bool         getIsPublic         (void);
        static void         setCluster          (const char* cluster);
        static const char*  getCluster          (void);

    private:

        static int memfd;
        static print_func_t print_func;
        static int io_timeout;
        static int io_maxsize;
        static int64_t launch_time;
        static const char* environment_version;
        static bool is_public;
        static const char* cluster_name;
};

#endif  /* __osapi__ */
