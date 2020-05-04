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

#ifndef __limit_record__
#define __limit_record__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "RecordObject.h"
#include "OsApi.h"

/******************************************************************************
 * LIMIT RECORD CLASS
 ******************************************************************************/

class LimitRecord: public RecordObject
{
    public:

        static const int MAX_FIELD_NAME_SIZE = 64;
        static const int MAX_RECORD_NAME_SIZE = 64;

        typedef struct {
            bool    filter_id;
            bool    limit_min;
            bool    limit_max;
            long    id;
            double  d_min;
            double  d_max;
            double  d_val;
            char    field_name[MAX_FIELD_NAME_SIZE];
            char    record_name[MAX_RECORD_NAME_SIZE];
        } limit_t;

        limit_t* limit;

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        LimitRecord(void);
        LimitRecord(limit_t _limit);
        ~LimitRecord(void);
};

#endif  /* __limit_record__ */
