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
        LocalLib::copy(src, _src, _src_size);
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
