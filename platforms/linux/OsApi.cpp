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

int OsApi::memfd = -1;
OsApi::print_func_t OsApi::print_func = NULL;
int OsApi::io_timeout = IO_DEFAULT_TIMEOUT;
int OsApi::io_maxsize = IO_DEFAULT_MAXSIZE;
int64_t OsApi::launch_time = 0;
char* OsApi::environment_version = NULL;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initLib
 *----------------------------------------------------------------------------*/
void OsApi::init(print_func_t _print_func)
{
    memfd = open("/proc/meminfo", O_RDONLY);
    launch_time = OsApi::time(OsApi::SYS_CLK);
    OsApi::dupstr(&environment_version, "unknown");
    print_func = _print_func;
}

/*----------------------------------------------------------------------------
 * deinitLib
 *----------------------------------------------------------------------------*/
void OsApi::deinit(void)
{
    if(memfd) close(memfd);
}

/*----------------------------------------------------------------------------
 * sleep
 *----------------------------------------------------------------------------*/
void OsApi::sleep(double secs)
{
    struct timespec waittime;

    waittime.tv_sec  = (time_t)secs;
    waittime.tv_nsec = (long)((secs - (long)secs) * 1000000000L);

    while( nanosleep(&waittime, &waittime) == -1 ) continue;
}

/*----------------------------------------------------------------------------
 *  dupstr
 *----------------------------------------------------------------------------*/
void OsApi::dupstr (char** dst, const char* src)
{
    assert(dst);
    if(*dst) delete [] *dst;
    int len = 0;
    while( (len < (MAX_STR_SIZE - 1)) && (src[len] != '\0') ) len++;
    *dst = new char[len + 1];
    for(int k = 0; k < len; k++) (*dst)[k] = src[k];
    (*dst)[len] = '\0';
}

/*----------------------------------------------------------------------------
*  time
*
*   SYS_CLK returns microseconds since unix epoch
*   CPU_CLK returns monotonically incrementing time normalized number (us)
*-----------------------------------------------------------------------------*/
int64_t OsApi::time(int clkid)
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
int64_t OsApi::timeres(int clkid)
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
uint16_t OsApi::swaps(uint16_t val)
{
    return bswap_16(val);
}

/*----------------------------------------------------------------------------
 * swapl
 *----------------------------------------------------------------------------*/
uint32_t OsApi::swapl(uint32_t val)
{
    return bswap_32(val);
}

/*----------------------------------------------------------------------------
 * swapll
 *----------------------------------------------------------------------------*/
uint64_t OsApi::swapll(uint64_t val)
{
    return bswap_64(val);
}

/*----------------------------------------------------------------------------
 * swapf
 *----------------------------------------------------------------------------*/
float OsApi::swapf(float val)
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
double OsApi::swaplf(double val)
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
int OsApi::nproc (void)
{
    return get_nprocs();
}

/*----------------------------------------------------------------------------
 * memusage
 *----------------------------------------------------------------------------*/
double OsApi::memusage (void)
{
    const int BUFSIZE = 128;
    const char* mem_total_ptr = NULL;
    const char* mem_available_ptr = NULL;
    int i = 0;
    char buffer[BUFSIZE];
    char *endptr;

    if(memfd)
    {
        lseek(memfd, 0, SEEK_SET);
        int bytes_read = read(memfd, buffer, BUFSIZE - 1);
        if(bytes_read > 0)
        {
            buffer[bytes_read] = '\0';

            /* Find MemTotal */
            i = 9; // start after colon
            while(buffer[i] == ' ') i++; // moves one past space
            mem_total_ptr = &buffer[i]; // mark start
            while(buffer[i] != ' ') i++; // stays at first space
            buffer[i] = '\0'; // mark end

            /* Find MemAvailable */
            i += 11; // move to colon after MemFree
            while(buffer[i++] != 'B'); // moves one past MemFree line
            i += 12;
            while(buffer[i++] != ':'); // moves one past colon
            while(buffer[i] == ' ') i++; // moves one past space
            mem_available_ptr = &buffer[i]; // mark start
            while(buffer[i] != ' ') i++; // stays at first space
            buffer[i] = '\0'; // mark end

            /* Convert MemTotal */
            errno = 0;
            long mem_total = strtol(mem_total_ptr, &endptr, 10);
            if( (endptr == mem_total_ptr) ||
                (errno == ERANGE && (mem_total == LONG_MAX || mem_total == LONG_MIN)) )
            {
                return 0.0;
            }

            /* Convert MemAvailable */
            errno = 0;
            long mem_available = strtol(mem_available_ptr, &endptr, 10);
            if( (endptr == mem_available_ptr) ||
                (errno == ERANGE && (mem_available == LONG_MAX || mem_available == LONG_MIN)) )
            {
                return 0.0;
            }

            /* Check Values */
            if(mem_total == 0 || mem_available > mem_total)
            {
                return 0.0;
            }

            /* Calculate Memory Usage */
            return 1.0 - ((double)mem_available / (double)mem_total);
        }
        else
        {
            return 0.0;
        }
    }
    else
    {
        return 0.0;
    }
}

/*----------------------------------------------------------------------------
 * print
 *----------------------------------------------------------------------------*/
void OsApi::print (const char* file_name, unsigned int line_number, const char* format_string, ...)
{
    /* Allocate Message String on Stack */
    char message[MAX_PRINT_MESSAGE];

    /* Build Formatted Message String */
    va_list args;
    va_start(args, format_string);
    vsnprintf(message, MAX_PRINT_MESSAGE, format_string, args);
    va_end(args);
    message[MAX_PRINT_MESSAGE - 1] = '\0';

    if(print_func)
    {
        /* Call Callback */
        print_func(file_name, line_number, message);
    }
    else
    {
        /* Default */
        printf("%s:%d %s\n", file_name, line_number, message);
    }
}

/*----------------------------------------------------------------------------
 * setIOMaxsize
 *----------------------------------------------------------------------------*/
bool OsApi::setIOMaxsize(int maxsize)
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
int OsApi::getIOMaxsize(void)
{
    return io_maxsize;
}

/*----------------------------------------------------------------------------
 * setIOTimeout
 *----------------------------------------------------------------------------*/
void OsApi::setIOTimeout(int timeout)
{
    io_timeout = timeout;
}

/*----------------------------------------------------------------------------
 * getIOTimeout
 *----------------------------------------------------------------------------*/
int OsApi::getIOTimeout(void)
{
    return io_timeout;
}

/*----------------------------------------------------------------------------
 * performIOTimeout
 *----------------------------------------------------------------------------*/
int OsApi::performIOTimeout(void)
{
    if(io_timeout >= 1000) OsApi::sleep(io_timeout / 1000);
    else                   OsApi::sleep(1);
    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * getLaunchTime
 *----------------------------------------------------------------------------*/
int64_t OsApi::getLaunchTime (void)
{
    return launch_time;
}

/*----------------------------------------------------------------------------
 * setEnvVersion
 *----------------------------------------------------------------------------*/
void OsApi::setEnvVersion (const char* verstr)
{
    OsApi::dupstr(&environment_version, verstr);
}

/*----------------------------------------------------------------------------
 * getEnvVersion
 *----------------------------------------------------------------------------*/
const char* OsApi::getEnvVersion (void)
{
    return environment_version;
}
