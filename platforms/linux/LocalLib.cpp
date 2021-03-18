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
void LocalLib::sleep(double secs)
{
    struct timespec waittime;

    waittime.tv_sec  = (time_t)secs;
    waittime.tv_nsec = (long)((secs - (long)secs) * 1000000000L);

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
