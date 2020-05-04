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
#ifndef __cmd_echo_processor_module__
#define __cmd_echo_processor_module__

#include "atlasdefines.h"
#include "ItosRecordParser.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class CmdEchoProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int ECHO_MSG_STR_SIZE = 2048;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CmdEchoProcessorModule  (CommandProcessor* cmd_proc, const char* obj_name, int pcenum, ItosRecordParser* itos_parser, const char* echoq_name);
                        ~CmdEchoProcessorModule (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        
        int                 pce;
        Publisher*          echoQ;  // output histograms
        ItosRecordParser*   itosParser;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments (List<CcsdsSpacePacket*>& segments, int numpkts);
};

#endif  /* __cmd_echo_processor_module__ */
