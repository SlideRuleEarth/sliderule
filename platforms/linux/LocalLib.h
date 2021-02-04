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

#ifndef __local_lib__
#define __local_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <pthread.h>

/******************************************************************************
 * LOCAL LIBRARY CLASS
 ******************************************************************************/

class LocalLib
{
    public:

        typedef pthread_key_t key_t;

        typedef void    (*print_func_t)         (const char* file_name, unsigned int line_number, const char* message);

        static const int SYS_CLK = 0; // system clock that can be converted into civil time
        static const int CPU_CLK = 1; // processor clock that only counts ticks
        static const int MAX_PRINT_MESSAGE = 256;
    
        static void         init                (void); // initializes library
        static void         deinit              (void); // de-initializes library
        static void         setPrint            (print_func_t _print_func);
        static void         print               (const char* file_name, unsigned int line_number, const char* format_string, ...)  __attribute__((format(printf, 3, 4)));
        static void         sleep               (int secs);
        static void*        copy                (void* dst, const void* src, int len);
        static void*        move                (void* dst, const void* src, int len);
        static void*        set                 (void* buf, int val, int len);
        static const char*  err2str             (int errnum);
        static int64_t      time                (int clkid);
        static int64_t      timeres             (int clkid); // returns the resolution of the clock
        static uint16_t     swaps               (uint16_t val);
        static uint32_t     swapl               (uint32_t val);
        static uint64_t     swapll              (uint64_t val);
        static float        swapf               (float val);
        static double       swaplf              (double val);
        static int          nproc               (void);
        static bool         setIOMaxsize        (int maxsize);
        static int          getIOMaxsize        (void);
        static void         setIOTimeout        (int timeout);
        static int          getIOTimeout        (void);
        static int          performIOTimeout    (void);

    private:
        
        static print_func_t print_func;
        static int io_timeout;
        static int io_maxsize;
};

#endif  /* __local_lib__ */
