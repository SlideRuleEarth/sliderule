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

#ifndef __ccsds_process_module__
#define __ccsds_process_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsPacket.h"
#include "CommandableObject.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PROCESSOR MODULE CLASS (PURE VIRTUAL)
 ******************************************************************************/

class CcsdsProcessorModule: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual bool    processSegments     (List<CcsdsSpacePacket*>& segments, int numpkts) = 0;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsProcessorModule        (CommandProcessor* cmd_proc, const char* obj_name);
                        ~CcsdsProcessorModule       (void);

        long            parseInt                    (unsigned char* ptr, int size);
        double          parseFlt                    (unsigned char* ptr, int size);
        double          integrateAverage            (uint32_t statcnt, double curr_avg, double new_val);
        double          integrateWeightedAverage    (uint32_t curr_cnt, double curr_avg, double new_val, uint32_t new_cnt);
};

#endif  /* __ccsds_processor_module__ */
