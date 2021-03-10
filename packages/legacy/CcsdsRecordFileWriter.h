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

#ifndef __ccsds_record_file_writer__
#define __ccsds_record_file_writer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsFileWriter.h"
#include "RecordObject.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS RECORD FILE WRITER CLASS
 ******************************************************************************/

class CcsdsRecordFileWriter: public CcsdsFileWriter
{
    public:

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    protected:

        char                    prevRecType[MAX_STR_SIZE];
        bool                    outputKeyValue;
        const char**            boundFields;
        int                     numBoundFields;
        

                                CcsdsRecordFileWriter   (CommandProcessor* cmd_proc, const char* name, const char* _prefix, const char* inq_name, const char** _bound_fields, int _num_bound_fields, unsigned int _max_file_size=FILE_MAX_SIZE);
                                ~CcsdsRecordFileWriter  (void);

        virtual int             writeMsg                (void* msg, int size, bool with_header=false);
        virtual RecordObject*   createRecord            (unsigned char* buffer, int size);
        virtual const char*     createPrependStr        (unsigned char* buffer, int size);

        int                     bindCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int                     outputKeyValueCmd       (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __ccsds_record_file_writer__ */
