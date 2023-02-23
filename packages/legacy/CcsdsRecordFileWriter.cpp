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

#include "CcsdsRecordFileWriter.h"
#include "ccsds.h"
#include "core.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CcsdsRecordFileWriter::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    const char* prefix = StringLib::checkNullStr(argv[0]);
    const char* stream = StringLib::checkNullStr(argv[1]);
    const char* filesize_str = NULL; // argv[2]

    if(prefix == NULL)
    {
        mlog(CRITICAL, "Error: prefix cannot be NULL");;
        return NULL;
    }

    if(stream == NULL)
    {
        mlog(CRITICAL, "Error: stream cannot be NULL");
        return NULL;
    }

    unsigned long filesize = CcsdsFileWriter::FILE_MAX_SIZE;
    if(argc > 2)
    {
        filesize_str = argv[2];
        if(!StringLib::str2ulong(filesize_str, &filesize))
        {
            mlog(CRITICAL, "Error: invalid file size: %s", filesize_str);
            return NULL;
        }
    }

    /* Field Bindings */
    const char** bound_fields = NULL;
    int num_bound_fields = 0;
    if(argc > 3)
    {
        num_bound_fields = argc - 3;
        bound_fields = new const char* [num_bound_fields];
        for(int i = 0; i < num_bound_fields; i++)
        {
            bound_fields[i] = StringLib::duplicate(argv[i + 3]);
        }
    }

    return new CcsdsRecordFileWriter(cmd_proc, name, prefix, stream, bound_fields, num_bound_fields, filesize);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CcsdsRecordFileWriter::CcsdsRecordFileWriter(CommandProcessor* cmd_proc, const char* obj_name, const char* _prefix, const char* inq_name, const char** _bound_fields, int _num_bound_fields, unsigned int _max_file_size):
    CcsdsFileWriter(cmd_proc, obj_name, CcsdsFileWriter::USER_DEFINED, _prefix, inq_name, _max_file_size)
{
    memset(prevRecType, 0, sizeof(prevRecType));
    outputKeyValue = false;
    boundFields = _bound_fields;
    numBoundFields = _num_bound_fields;

    registerCommand("OUTPUT_KEY_VALUE",  (cmdFunc_t)&CcsdsRecordFileWriter::outputKeyValueCmd,  0,  "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CcsdsRecordFileWriter::~CcsdsRecordFileWriter(void)
{
}

/*----------------------------------------------------------------------------
 * writeMsg -
 *----------------------------------------------------------------------------*/
int CcsdsRecordFileWriter::writeMsg(void* msg, int size, bool with_header)
{
    (void)with_header;

    /* Get Record */
    RecordObject* record = NULL;
    try
    {
        record = createRecord((unsigned char*)msg, size);
    }
    catch (const RunTimeException& e)
    {
        mlog(ERROR, "Failed to create record in %s: %s", getName(), e.what());
        return 0;
    }

    /* Get Fields */
    char** field_names = NULL;
    int num_fields = 0;
    if(boundFields)
    {
        num_fields = numBoundFields;
        field_names = (char**)boundFields;
    }
    else
    {
        num_fields = record->getFieldNames(&field_names);
    }

    int cnt = -1;
    if(num_fields > 0)
    {
        if(outputKeyValue) // key,value pairs
        {
            for(int i = 0; i < num_fields ; i++)
            {
                /* Output Prepended String */
                const char* prepend = createPrependStr((unsigned char*)msg, size);
                if(prepend)
                {
                    cnt += fprintf(outfp, "%s,", prepend);
                    delete prepend;
                }

                /* Output Name */
                cnt += fprintf(outfp, "%s,", field_names[i]);

                /* Output Value */
                RecordObject::field_t field = record->getField(field_names[i]);
                char valbuf[RecordObject::MAX_VAL_STR_SIZE];
                cnt += fprintf(outfp, "%s\n", record->getValueText(field, valbuf));
            }
        }
        else // csv
        {
            /* Write Header */
            if(!record->isRecordType(prevRecType))
            {
                StringLib::copy(prevRecType, record->getRecordType(), MAX_STR_SIZE);

                const char* prepend = createPrependStr(NULL, 0);
                if(prepend)
                {
                    cnt += fprintf(outfp, "%s,", prepend);
                    delete [] prepend;
                }

                for(int i = 0; i < num_fields - 1; i++)
                {
                    cnt += fprintf(outfp, "%s,", field_names[i]);
                }
                cnt += fprintf(outfp, "%s\n", field_names[num_fields - 1]);
            }

            /* Write Fields */
            const char* prepend = createPrependStr((unsigned char*)msg, size);
            if(prepend)
            {
                cnt += fprintf(outfp, "%s,", prepend);
                delete [] prepend;
            }
            for(int i = 0; i < num_fields - 1; i++)
            {
                RecordObject::field_t field = record->getField(field_names[i]);
                char valbuf[RecordObject::MAX_VAL_STR_SIZE];
                cnt += fprintf(outfp, "%s,", record->getValueText(field, valbuf));
            }
            RecordObject::field_t field = record->getField(field_names[num_fields - 1]);
            char valbuf[RecordObject::MAX_VAL_STR_SIZE];
            cnt += fprintf(outfp, "%s\n", record->getValueText(field, valbuf));
        }
    }

    /* Clean Up */
    if(!boundFields)
    {
        for(int i = 0; i < num_fields; i++)
        {
            delete [] field_names[i];
        }
        delete [] field_names;
    }
    delete record;

    return cnt;
}

/*----------------------------------------------------------------------------
 * createRecord
 *----------------------------------------------------------------------------*/
RecordObject* CcsdsRecordFileWriter::createRecord (unsigned char* buffer, int size)
{
    return new CcsdsRecordInterface(buffer, size);
}

/*----------------------------------------------------------------------------
 * createPrependStr
 *----------------------------------------------------------------------------*/
const char* CcsdsRecordFileWriter::createPrependStr (unsigned char* buffer, int size)
{
    (void)size;

    if(buffer == NULL)
    {
        return StringLib::duplicate("GMT");
    }
    else
    {
        try
        {
#ifdef XINA
            char timestr[MAX_STR_SIZE];
            CcsdsPacket ccsdspkt(buffer, size);
            CcsdsPacket::gmt_time_t gmt_time = ccsdspkt.getCdsTimeAsGmt();
            double unix_time = gmt2unixtime(gmt_time);
            StringLib::format(timestr, MAX_STR_SIZE, "%ld,%02d:%03d:%02d:%02d:%02d,%d", (long)(unix_time * 1000000.0), gmt_time.year, gmt_time.doy, gmt_time.hour, gmt_time.minute, gmt_time.second, 0);
            return StringLib::duplicate(timestr);
#else
            CcsdsSpacePacket ccsdspkt(buffer, size);
            CcsdsSpacePacket::pkt_time_t gmt_time = ccsdspkt.getCdsTimeAsGmt();
            char gmtstr[MAX_STR_SIZE];
            StringLib::format(gmtstr, MAX_STR_SIZE, "%02d:%03d:%02d:%02d:%02d", gmt_time.year, gmt_time.doy, gmt_time.hour, gmt_time.minute, gmt_time.second);
            return StringLib::duplicate(gmtstr);
#endif
        }
        catch(RunTimeException& e)
        {
            (void)e;
            return "::::";
        }
    }
}

/*----------------------------------------------------------------------------
 * outputKeyValueCmd  -
 *----------------------------------------------------------------------------*/
int CcsdsRecordFileWriter::outputKeyValueCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    outputKeyValue = true;

    return 0;
}
