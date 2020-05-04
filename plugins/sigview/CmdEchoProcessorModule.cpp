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

#include "CmdEchoProcessorModule.h"
#include "ItosRecordParser.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CmdEchoProcessorModule::CmdEchoProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, int pcenum, ItosRecordParser* itos_parser, const char* echoq_name):
    CcsdsProcessorModule(cmd_proc, obj_name),
    pce(pcenum),
    itosParser(itos_parser)
{
    assert(echoq_name);

    /* Initialize Streams */
    echoQ = new Publisher(echoq_name);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CmdEchoProcessorModule::~CmdEchoProcessorModule(void)
{
    delete echoQ;
}

/******************************************************************************
 * PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* CmdEchoProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    const char* echoq_name = StringLib::checkNullStr(argv[0]);
    const char* itos_name  = StringLib::checkNullStr(argv[1]);
    int         pcenum     = NOT_PCE + 1;

    if(argc > 2)
    {
        pcenum = (int)strtol(argv[2], NULL, 0);
        if(pcenum < 1 || pcenum > NUM_PCES)
        {
            mlog(CRITICAL, "Invalid PCE specified: %d, must be between 1 and %d\n", pcenum, NUM_PCES);
            return NULL;
        }
    }

    if(echoq_name == NULL)
    {
        mlog(CRITICAL, "Echo queue cannot be null!\n");
        return NULL;
    }

    ItosRecordParser* itos = NULL;
    if(itos_name != NULL)
    {
        itos = (ItosRecordParser*)cmd_proc->getObject(itos_name, ItosRecordParser::TYPE);
        if(itos == NULL)
        {
            mlog(CRITICAL, "Unable to locate ITOS record parser: %s\n", itos_name);
            return NULL;
        }
    }

    return new CmdEchoProcessorModule(cmd_proc, name, pcenum - 1, itos, echoq_name);
}

/******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parse Command Echo Packets
 *----------------------------------------------------------------------------*/
bool CmdEchoProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    char                    echo_msg[ECHO_MSG_STR_SIZE];
    char                    task_prefix[8];
    bool                    status;
    unsigned char*          cmd_pkt;

    /* Process Segments */
    int numsegs = segments.length();
    for(int p = 0; p < numsegs; p++)
    {
        CcsdsSpacePacket* ccsdspkt = segments[p];
        unsigned char* pktbuf = ccsdspkt->getBuffer();
        TimeLib::gmt_time_t gmt_time = ccsdspkt->getCdsTimeAsGmt();

        /* Pull Out Fields */
        memset(task_prefix, 0, 8);
        LocalLib::copy(task_prefix, pktbuf + 12, 7);
        status = pktbuf[19] == 0 ? false : true;
        cmd_pkt = &pktbuf[20];

        /* Print Prolog */
        if(pce == NOT_PCE)
        {
            snprintf(echo_msg, ECHO_MSG_STR_SIZE, "[SBC   CMD] %02d:%03d:%02d:%02d:%02d <%s> %s: ",
                    gmt_time.year, gmt_time.day,
                    gmt_time.hour, gmt_time.minute, gmt_time.second,
                    task_prefix, status ? "ACCEPTED" : "REJECTED");
        }
        else
        {
            snprintf(echo_msg, ECHO_MSG_STR_SIZE, "[PCE %d CMD] %02d:%03d:%02d:%02d:%02d <%s> %s: ",
                    gmt_time.year, gmt_time.day,
                    gmt_time.hour, gmt_time.minute, gmt_time.second,
                    pce + 1, task_prefix, status ? "ACCEPTED" : "REJECTED");
        }

        /* Attempt to Pretty Print Command Echo */
        bool printed = false;
        if(itosParser != NULL)
        {
            const char* pretty_print = itosParser->pkt2str(cmd_pkt);
            if(pretty_print != NULL)
            {
                StringLib::concat(echo_msg, pretty_print, ECHO_MSG_STR_SIZE);
                StringLib::concat(echo_msg, "\n", ECHO_MSG_STR_SIZE);
                delete [] pretty_print;
                printed = true;
            }
        }

        /* Otherwise Print Raw */
        if(printed == false)
        {
            int len = MIN(CCSDS_GET_LEN(cmd_pkt), 256 - 20);
            for(int k = 0; k < len; k++)
            {
                char valstr[3];
                snprintf(valstr, 3, "%02X", cmd_pkt[k]);
                StringLib::concat(echo_msg, valstr, ECHO_MSG_STR_SIZE);
            }
            StringLib::concat(echo_msg, "\n", ECHO_MSG_STR_SIZE);
        }

        /* Print Echo */
        int echo_status = echoQ->postCopy(echo_msg, strlen(echo_msg) + 1, SYS_TIMEOUT);
        if(echo_status <= 0)
        {
            mlog(ERROR, "Failed to post echoed command with status %d: %s", echo_status, echo_msg);
        }
    }

    return true;
}
