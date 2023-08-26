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

#include "ContainerRecord.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ContainerRecord::entryRecType = "conrec.entry";
RecordObject::fieldDef_t ContainerRecord::entryRecDef[] =
{
    {"size",        UINT32, offsetof(entry_t, rec_size),        1,   NULL, NATIVE_FLAGS},
    {"offset",      UINT32, offsetof(entry_t, rec_offset),      1,   NULL, NATIVE_FLAGS}
};

const char* ContainerRecord::recType = "conrec";
RecordObject::fieldDef_t ContainerRecord::recDef[] =
{
    {"count",       UINT32, offsetof(rec_t, rec_cnt),           1,   NULL, NATIVE_FLAGS},
    {"records",     USER,   offsetof(rec_t, entries),           0,   entryRecType, NATIVE_FLAGS} // variable length
};

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ContainerRecord::init (void)
{
    RECDEF(recType,       recDef,       sizeof(rec_t),      NULL);
    RECDEF(entryRecType,  entryRecDef,  sizeof(entry_t),    NULL);
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
int ContainerRecord::hdrSize (int cnt)
{
    return (sizeof(entry_t) * cnt) + offsetof(rec_t, entries);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ContainerRecord::ContainerRecord(int rec_cnt, int size):
    RecordObject(recType, size + hdrSize(rec_cnt))
{
    recsContained = 0;
    recsOffset = hdrSize(rec_cnt);
    container = (rec_t*)recordData;
    container->rec_cnt = rec_cnt;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ContainerRecord::~ContainerRecord()
{
}    

/*----------------------------------------------------------------------------
 * addRecord
 *----------------------------------------------------------------------------*/
bool ContainerRecord::addRecord(RecordObject& record, int size)
{
    if(recsContained < container->rec_cnt)
    {
        uint8_t* rec_buf = NULL;
        int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE, size);

        /* populate entry */
        container->entries[recsContained].rec_offset = recsOffset;
        container->entries[recsContained].rec_size = rec_bytes;

        /* copy record memory into container */
        uint8_t* rec_ptr = recordData + recsOffset;
        memcpy(rec_ptr, rec_buf, rec_bytes);
        recsOffset += rec_bytes;

        /* bump number of records contained */
        recsContained++;

        return true;
    }
    else
    {
        return false;
    }

}
