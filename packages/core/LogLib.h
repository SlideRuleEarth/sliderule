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

#ifndef __loglib__
#define __loglib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Ordering.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifdef _WINDOWS_
#undef ERROR
#endif

typedef enum {
    INVALID_LOG_LEVEL = -1,
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4,
    RAW = 5
} log_lvl_t;

#define mlog(lvl,...) LogLib::logMsg(__FILE__,__LINE__,lvl,__VA_ARGS__)

/******************************************************************************
 * LOG LIBRARY CLASS
 ******************************************************************************/

class LogLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_LOG_ENTRY_SIZE = 512;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef int (*logFunc_t) (const char* str, int size, void* parm);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static  void        init        (void);
        static  void        deinit      (void);
        static  okey_t      createLog   (log_lvl_t lvl, logFunc_t handler, void* parm);
        static  bool        deleteLog   (okey_t id);
        static  bool        setLevel    (okey_t id, log_lvl_t lvl);
        static  log_lvl_t   getLevel    (okey_t id);
        static  int         getLvlCnts  (log_lvl_t lvl);
        static  bool        str2lvl     (const char* str, log_lvl_t* lvl);
        static  void        logMsg      (const char* file_name, unsigned int line_number, log_lvl_t lvl, const char* format_string, ...) VARG_CHECK(printf, 4, 5);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct
        {
            okey_t      id;
            log_lvl_t   level;
            logFunc_t   handler;
            void*       parm;
        } log_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static okey_t           logIdPool;
        static Ordering<log_t>  logList;
        static Mutex            logMut;
        static int              logLvlCnts[RAW + 1];
};

#endif  /* __loglib__ */
