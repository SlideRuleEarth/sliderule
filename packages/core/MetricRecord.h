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

#ifndef __metric_record__
#define __metric_record__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
#include "Ordering.h"   // for okey_t index which is intended to be compatible with an ordering
#include "OsApi.h"

/******************************************************************************
 * METRIC RECORD CLASS
 ******************************************************************************/

class MetricRecord: public RecordObject
{
    public:

        typedef struct {
            okey_t      index;          // ordered key of record, usually time
            double      value;          // value of field in record as double precision
            int32_t     text_offset;    // value of field in record as text string
            int32_t     name_offset;    // <record type>.<field name>
            int32_t     src_offset;     // pointer to serialized source record
            int32_t     src_size;       // size of serialized source record
            uint64_t    size;           // size of entire serialized record including type (since it is variable length)
        } metric_t;

        metric_t*   metric;
        char*       text;
        char*       name;
        void*       src;

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        MetricRecord(okey_t _index, double _value, const char* _text, const char* _name, void* _src, int _src_size);
        ~MetricRecord(void);

        static int calcRecordSize (const char* _text, const char* _name, int _src_size);
};

#endif  /* __metric_record__ */
