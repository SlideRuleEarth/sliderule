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

#include "MetricRecord.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MetricRecord::rec_type = "Metric";

RecordObject::fieldDef_t MetricRecord::rec_def[] =
{
    {"INDEX",       UINT64, offsetof(metric_t, index),          1,   NULL, NATIVE_FLAGS},
    {"VALUE",       DOUBLE, offsetof(metric_t, value),          1,   NULL, NATIVE_FLAGS},
    {"TEXT",        STRING, offsetof(metric_t, text_offset),    1,   NULL, NATIVE_FLAGS | RecordObject::POINTER},
    {"NAME",        STRING, offsetof(metric_t, name_offset),    1,   NULL, NATIVE_FLAGS | RecordObject::POINTER},
    {"SOURCE",      STRING, offsetof(metric_t, src_offset),     1,   NULL, NATIVE_FLAGS | RecordObject::POINTER},
    {"SRC_SIZE",    INT32,  offsetof(metric_t, src_size),       1,   NULL, NATIVE_FLAGS}
};

int MetricRecord::rec_elem = sizeof(MetricRecord::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MetricRecord::MetricRecord(okey_t _index, double _value, const char* _text, const char* _name, void* _src, int _src_size):
    RecordObject(rec_type, calcRecordSize(_text, _name, _src_size))
{
    /* Populate Initial Metric */
    metric = (metric_t*)recordData;
    metric->index = _index;
    metric->value = _value;
    metric->text_offset = 0;
    metric->name_offset = 0;
    metric->src_offset = 0;
    metric->src_size = _src_size;
    metric->size = memoryAllocated;

    /* Populate Class Attributes */
    text = NULL;
    name = NULL;
    src = NULL;

    /* Populate Text */
    int text_size = 0;
    if(_text)
    {
        metric->text_offset = sizeof(metric_t);
        text = (char*)(recordData + metric->text_offset);
        text_size = StringLib::size(_text) + 1;
        StringLib::copy(text, _text, text_size);
    }

    /* Populate Name */
    int name_size = 0;
    if(_name)
    {
        metric->name_offset = metric->text_offset + text_size;
        name = (char*)(recordData + metric->name_offset);
        name_size = StringLib::size(_name) + 1;
        StringLib::copy(name, _name, name_size);
    }

    /* Populate Source */
    if(_src)
    {
        metric->src_offset = metric->name_offset + name_size;
        src = recordData + metric->src_offset;
        memcpy(src, _src, _src_size);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MetricRecord::~MetricRecord()
{
}

/*----------------------------------------------------------------------------
 * calcRecordSize
 *
 *  Static function to caclulate the size of the record for RecordObject initialization
 *----------------------------------------------------------------------------*/
int MetricRecord::calcRecordSize (const char* _text, const char* _name, int _src_size)
{
    int text_len = StringLib::size(_text) + 1;
    int name_len = StringLib::size(_name) + 1;
    return (sizeof(metric_t) + text_len + name_len + _src_size);
}
