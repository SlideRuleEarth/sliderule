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

#ifndef __ccsds_publisher_processor_module__
#define __ccsds_publisher_processor_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsProcessorModule.h"
#include "MsgQ.h"
#include "List.h"

/******************************************************************************
 * CCSDS PUBLISHER PROCESSOR MODULE
 ******************************************************************************/

class CcsdsPublisherProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);
        static void freePkt (void* obj, void* parm);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Publisher*          pubQ;
        bool                concatSegments;
        bool                checkLength;
        bool                checkChecksum;        
        int                 stripHeaderBytes;
        
        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                CcsdsPublisherProcessorModule   (CommandProcessor* cmd_proc, const char* obj_name, const char* pubq_name);
                ~CcsdsPublisherProcessorModule  (void);

        bool    processSegments                 (List<CcsdsSpacePacket*>& segments, int numpkts);

        int     concatSegmentsCmd               (int argc, char argv[][MAX_CMD_SIZE]);
        int     checkLengthCmd                  (int argc, char argv[][MAX_CMD_SIZE]);
        int     checkChecksumCmd                (int argc, char argv[][MAX_CMD_SIZE]);
        int     stripHeaderCmd                  (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __ccsds_publisher_processor_module__ */