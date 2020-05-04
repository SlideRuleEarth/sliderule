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

#include "LimitRecord.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LimitRecord::rec_type = "Limit";

RecordObject::fieldDef_t LimitRecord::rec_def[] =
{
    {"FILTER_ID",   UINT8,  offsetof(limit_t, filter_id),   sizeof(((limit_t*)0)->filter_id), NATIVE_FLAGS},
    {"LIMIT_MIN",   UINT8,  offsetof(limit_t, limit_min),   sizeof(((limit_t*)0)->limit_min), NATIVE_FLAGS},
    {"LIMIT_MAX",   UINT8,  offsetof(limit_t, limit_max),   sizeof(((limit_t*)0)->limit_max), NATIVE_FLAGS},
    {"ID",          INT64,  offsetof(limit_t, id),          sizeof(((limit_t*)0)->id),        NATIVE_FLAGS},
    {"D_MIN",       DOUBLE, offsetof(limit_t, d_min),       sizeof(((limit_t*)0)->d_min),     NATIVE_FLAGS},
    {"D_MAX",       DOUBLE, offsetof(limit_t, d_max),       sizeof(((limit_t*)0)->d_max),     NATIVE_FLAGS},
    {"D_VAL",       DOUBLE, offsetof(limit_t, d_val),       sizeof(((limit_t*)0)->d_val),     NATIVE_FLAGS},
    {"FIELD_NAME",  STRING, offsetof(limit_t, field_name),  MAX_FIELD_NAME_SIZE,              NATIVE_FLAGS},
    {"RECORD_NAME", STRING, offsetof(limit_t, record_name), MAX_RECORD_NAME_SIZE,             NATIVE_FLAGS}
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
