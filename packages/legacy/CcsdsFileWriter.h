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

#ifndef __ccsds_file_writer__
#define __ccsds_file_writer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "CcsdsMsgProcessor.h"

/******************************************************************************
 * CCSDS FILE WRITER CLASS
 ******************************************************************************/

class CcsdsFileWriter: public CcsdsMsgProcessor
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/


        static const unsigned int FILE_MAX_SIZE = 0x8000000;
        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            RAW_BINARY,
            RAW_ASCII,
            TEXT,
            USER_DEFINED,
            INVALID
        } fmt_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        static fmt_t        str2fmt     (const char* str);
        static const char*  fmt2str     (fmt_t _fmt);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const unsigned int FILENAME_MAX_CHARS = 512;

        fmt_t               fmt;
        char*               prefix;
        char                filename[FILENAME_MAX_CHARS];
        FILE*               outfp;
        unsigned long       recordsWritten;
        unsigned long       fileCount;
        unsigned long       fileBytesWritten;
        unsigned int        maxFileSize;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsFileWriter     (CommandProcessor* cmd_proc, const char* name, fmt_t fmt, const char* _prefix, const char* inq_name, unsigned int _maxFileSize=FILE_MAX_SIZE);
        virtual         ~CcsdsFileWriter    (void);

        virtual bool    openNewFile         (void);
        virtual int     writeMsg            (void* msg, int size, bool with_header=false);
        virtual bool    isBinary            (void);

                bool    processMsg          (unsigned char* msg, int bytes); // OVERLOAD

                int     flushCmd            (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __ccsds_file_writer__ */
