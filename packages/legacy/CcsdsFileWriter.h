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
