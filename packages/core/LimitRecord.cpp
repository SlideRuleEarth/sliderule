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

#include "LimitRecord.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LimitRecord::rec_type = "Limit";

RecordObject::fieldDef_t LimitRecord::rec_def[] =
{
    {"FILTER_ID",   UINT8,  offsetof(limit_t, filter_id),   1,                      NULL, NATIVE_FLAGS},
    {"LIMIT_MIN",   UINT8,  offsetof(limit_t, limit_min),   1,                      NULL, NATIVE_FLAGS},
    {"LIMIT_MAX",   UINT8,  offsetof(limit_t, limit_max),   1,                      NULL, NATIVE_FLAGS},
    {"ID",          INT64,  offsetof(limit_t, id),          1,                      NULL, NATIVE_FLAGS},
    {"D_MIN",       DOUBLE, offsetof(limit_t, d_min),       1,                      NULL, NATIVE_FLAGS},
    {"D_MAX",       DOUBLE, offsetof(limit_t, d_max),       1,                      NULL, NATIVE_FLAGS},
    {"D_VAL",       DOUBLE, offsetof(limit_t, d_val),       1,                      NULL, NATIVE_FLAGS},
    {"FIELD_NAME",  STRING, offsetof(limit_t, field_name),  MAX_FIELD_NAME_SIZE,    NULL, NATIVE_FLAGS},
    {"RECORD_NAME", STRING, offsetof(limit_t, record_name), MAX_RECORD_NAME_SIZE,   NULL, NATIVE_FLAGS}
};

int LimitRecord::rec_elem = sizeof(LimitRecord::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LimitRecord::LimitRecord(void): RecordObject(rec_type)
{
    limit = (limit_t*)recordData;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LimitRecord::LimitRecord(limit_t _limit): RecordObject(rec_type)
{
    limit = (limit_t*)recordData;
    *limit = _limit;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LimitRecord::~LimitRecord()
{
}
