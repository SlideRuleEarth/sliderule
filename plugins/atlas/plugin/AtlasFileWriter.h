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

#ifndef __atlas_file_writer__
#define __atlas_file_writer__

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class AtlasFileWriter: public CcsdsFileWriter
{
    public:

        typedef enum {
            SCI_PKT,
            SCI_CH,
            SCI_TX,
            HISTO,
            CCSDS_STAT,
            CCSDS_INFO,
            META,
            CHANNEL,
            AVCPT,
            TIMEDIAG,
            TIMESTAT,
            INVALID
        } fmt_t;

                                    AtlasFileWriter     (CommandProcessor* cmd_proc, const char* name, fmt_t _fmt, const char* _prefix, const char* inq_name, unsigned int _max_file_size=FILE_MAX_SIZE);
        virtual                     ~AtlasFileWriter    (void);

        static CommandableObject*   createObject        (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        static fmt_t                str2fmt             (const char* str);
        static const char*          fmt2str             (fmt_t _fmt);

    protected:

        int     writeMsg        (void* msg, int size, bool with_header=false);
        bool    isBinary        (void);

    private:

        fmt_t   fmt;

        int writeSciPkt         (void* record, int size, bool with_header=false);
        int writeSciCh          (void* record, int size, bool with_header=false);
        int writeSciTx          (void* record, int size, bool with_header=false);
        int writeHisto          (void* record, int size, bool with_header=false);
        int writeCcsdsStat      (void* record, int size, bool with_header=false);
        int writeCcsdsInfo      (void* record, int size, bool with_header=false);
        int writeHistoMeta      (void* record, int size, bool with_header=false);
        int writeHistoChannel   (void* record, int size, bool with_header=false);
        int writeAVCPT          (void* record, int size, bool with_header=false);
        int writeTimeDiag       (void* record, int size, bool with_header=false);
        int writeTimeStat       (void* record, int size, bool with_header=false);
};

#endif  /* __atlas_file_writer__ */
