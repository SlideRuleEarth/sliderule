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

        typedef struct limit {
            bool    filter_id;
            bool    limit_min;
            bool    limit_max;
            long    id;
            double  d_min;
            double  d_max;
            double  d_val;
            char    field_name[MAX_FIELD_NAME_SIZE];
            char    record_name[MAX_RECORD_NAME_SIZE];

            limit() {clear();}

            void clear(void)
            {
                filter_id = false;
                limit_min = false;
                limit_max = false;
                id = 0;
                d_min = 0.0;
                d_max = 0.0;
                d_val = 0.0;
                memset(field_name, 0, MAX_FIELD_NAME_SIZE);
                memset(record_name, 0, MAX_RECORD_NAME_SIZE);
            }
        } limit_t;

        limit_t* limit;

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        LimitRecord(void);
        explicit LimitRecord(const limit_t& _limit);
        ~LimitRecord(void) override;
};

#endif  /* __limit_record__ */
