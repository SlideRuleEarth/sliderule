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
