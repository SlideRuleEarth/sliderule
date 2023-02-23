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
            mlog(CRITICAL, "Invalid PCE specified: %d, must be between 1 and %d", pcenum, NUM_PCES);
            return NULL;
        }
    }

    if(echoq_name == NULL)
    {
        mlog(CRITICAL, "Echo queue cannot be null!");
        return NULL;
    }

    ItosRecordParser* itos = NULL;
    if(itos_name != NULL)
    {
        itos = (ItosRecordParser*)cmd_proc->getObject(itos_name, ItosRecordParser::TYPE);
        if(itos == NULL)
        {
            mlog(CRITICAL, "Unable to locate ITOS record parser: %s", itos_name);
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
        memcpy(task_prefix, pktbuf + 12, 7);
        status = pktbuf[19] == 0 ? false : true;
        cmd_pkt = &pktbuf[20];

        /* Print Prolog */
        if(pce == NOT_PCE)
        {
            snprintf(echo_msg, ECHO_MSG_STR_SIZE, "[SBC   CMD] %02d:%03d:%02d:%02d:%02d <%s> %s: ",
                    gmt_time.year, gmt_time.doy,
                    gmt_time.hour, gmt_time.minute, gmt_time.second,
                    task_prefix, status ? "ACCEPTED" : "REJECTED");
        }
        else
        {
            snprintf(echo_msg, ECHO_MSG_STR_SIZE, "[PCE %d CMD] %02d:%03d:%02d:%02d:%02d <%s> %s: ",
                    pce + 1,
                    gmt_time.year, gmt_time.doy,
                    gmt_time.hour, gmt_time.minute, gmt_time.second,
                    task_prefix, status ? "ACCEPTED" : "REJECTED");
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
