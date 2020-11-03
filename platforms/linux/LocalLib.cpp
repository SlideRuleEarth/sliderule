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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <byteswap.h>
#include <sys/sysinfo.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

LocalLib::print_func_t LocalLib::print_func = NULL;
int LocalLib::io_timeout = IO_DEFAULT_TIMEOUT;
int LocalLib::io_maxsize = IO_DEFAULT_MAXSIZE;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initLib
 *----------------------------------------------------------------------------*/
void LocalLib::init()
{
}

/*----------------------------------------------------------------------------
 * deinitLib
 *----------------------------------------------------------------------------*/
void LocalLib::deinit(void)
{
}

/*----------------------------------------------------------------------------
 * setPrint
 *----------------------------------------------------------------------------*/
void LocalLib::setPrint(print_func_t _print_func)
{
    assert(_print_func);    // do not let print function be set to NULL
    print_func = _print_func;
}

/*----------------------------------------------------------------------------
 * print
 *----------------------------------------------------------------------------*/
void LocalLib::print (const char* file_name, unsigned int line_number, const char* format_string, ...)
{
    if(print_func)
    {
        /* Allocate Message String on Stack */
        char message[MAX_PRINT_MESSAGE];

        /* Build Formatted Message String */
        va_list args;
        va_start(args, format_string);
        vsnprintf(message, MAX_PRINT_MESSAGE, format_string, args);
        va_end(args);
        message[MAX_PRINT_MESSAGE - 1] = '\0';

        /* Call Call Back */
        print_func(file_name, line_number, message);
    }
}

/*----------------------------------------------------------------------------
 * sleep
 *----------------------------------------------------------------------------*/
void LocalLib::sleep(int secs)
{
    struct timespec waittime;

    waittime.tv_sec  = secs;
    waittime.tv_nsec = 0;

    while( nanosleep(&waittime, &waittime) == -1 ) continue;
}

/*----------------------------------------------------------------------------
 *  copy
 *----------------------------------------------------------------------------*/
void* LocalLib::copy(void* dst, const void* src, int len)
{
    return memcpy(dst, src, len);
}

/*----------------------------------------------------------------------------
 *  move
 *----------------------------------------------------------------------------*/
void* LocalLib::move(void* dst, const void* src, int len)
{
    return memmove(dst, src, len);
}

/*----------------------------------------------------------------------------
 *  set
 *----------------------------------------------------------------------------*/
void* LocalLib::set(void* buf, int val, int len)
{
    return memset(buf, val, len);
}

/*----------------------------------------------------------------------------
 *  err2str
 *----------------------------------------------------------------------------*/
const char* LocalLib::err2str(int errnum)
{
    return strerror(errnum);
}

/*----------------------------------------------------------------------------
*  time
*
*   SYS_CLK returns microseconds since unix epoch
*   CPU_CLK returns monotonically incrementing time normalized number (us)
*-----------------------------------------------------------------------------*/
int64_t LocalLib::time(int clkid)
{
    struct timespec now;

    if (clkid == SYS_CLK)
    {
        clock_gettime(CLOCK_REALTIME, &now);
    }
    else if (clkid == CPU_CLK)
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
    }
    else
    {
        return 0;
    }

    return ((int64_t)now.tv_sec * 1000000) + (now.tv_nsec / 1000);
}

/*----------------------------------------------------------------------------
* timeres
*
* Returns: resolution of specified clock in number of ticks per second
*-----------------------------------------------------------------------------*/
int64_t LocalLib::timeres(int clkid)
{
    if (clkid == SYS_CLK)
    {
        return 1000000; // microseconds
    }
    else if (clkid == CPU_CLK)
    {
        return 1000000; // microseconds
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * swaps
 *----------------------------------------------------------------------------*/
uint16_t LocalLib::swaps(uint16_t val)
{
    return bswap_16(val);
}

/*----------------------------------------------------------------------------
 * swapl
 *----------------------------------------------------------------------------*/
uint32_t LocalLib::swapl(uint32_t val)
{
    return bswap_32(val);
}

/*----------------------------------------------------------------------------
 * swapll
 *----------------------------------------------------------------------------*/
uint64_t LocalLib::swapll(uint64_t val)
{
    return bswap_64(val);
}

/*----------------------------------------------------------------------------
 * swapf
 *----------------------------------------------------------------------------*/
float LocalLib::swapf(float val)
{
    float rtrndata = 0.0;
    uint8_t* dataptr = (uint8_t*)&rtrndata;
    uint32_t* tempf = (uint32_t*)&val;
    *(uint32_t*)(dataptr) = bswap_32(*tempf);
    return rtrndata;
}

/*----------------------------------------------------------------------------
 * swaplf
 *----------------------------------------------------------------------------*/
double LocalLib::swaplf(double val)
{
    double rtrndata = 0.0;
    uint8_t* dataptr = (uint8_t*)&rtrndata;
    uint64_t* tempd = (uint64_t*)&val;
    *(uint64_t*)(dataptr) = bswap_64(*tempd);
    return rtrndata;
}

/*----------------------------------------------------------------------------
 * nproc
 *----------------------------------------------------------------------------*/
int LocalLib::nproc (void)
{
    return get_nprocs();
}

/*----------------------------------------------------------------------------
 * setIOMaxsize
 *----------------------------------------------------------------------------*/
bool LocalLib::setIOMaxsize(int maxsize)
{
    if(maxsize > 0)
    {
        io_maxsize = maxsize;
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getIOMaxsize
 *----------------------------------------------------------------------------*/
int LocalLib::getIOMaxsize(void)
{
    return io_maxsize;
}

/*----------------------------------------------------------------------------
 * setIOTimeout
 *----------------------------------------------------------------------------*/
void LocalLib::setIOTimeout(int timeout)
{
    io_timeout = timeout;
}

/*----------------------------------------------------------------------------
 * getIOTimeout
 *----------------------------------------------------------------------------*/
int LocalLib::getIOTimeout(void)
{
    return io_timeout;
}

/*----------------------------------------------------------------------------
 * performIOTimeout
 *----------------------------------------------------------------------------*/
int LocalLib::performIOTimeout(void)
{
    if(io_timeout >= 1000) LocalLib::sleep(io_timeout / 1000);
    else                   LocalLib::sleep(1);
    return TIMEOUT_RC;
}
