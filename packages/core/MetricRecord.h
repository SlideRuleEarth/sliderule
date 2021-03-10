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

#ifndef __metric_record__
#define __metric_record__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
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
