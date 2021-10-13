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
        static void         sleep               (double secs); // sleep at highest resolution available on system
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
        static double       memusage            (void);
        static bool         setIOMaxsize        (int maxsize);
        static int          getIOMaxsize        (void);
        static void         setIOTimeout        (int timeout);
        static int          getIOTimeout        (void);
        static int          performIOTimeout    (void);

    private:

        static print_func_t print_func;
        static int io_timeout;
        static int io_maxsize;
        static int memfd;
};

#endif  /* __local_lib__ */
