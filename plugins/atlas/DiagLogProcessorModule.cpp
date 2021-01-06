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

#include "DiagLogProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
DiagLogProcessorModule::DiagLogProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, const char* diagq_name, const char* _prefix):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    assert(diagq_name);

    diagQ = new Publisher(diagq_name);
    prefix = StringLib::duplicate(_prefix);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
DiagLogProcessorModule::~DiagLogProcessorModule(void)
{
    delete diagQ;
    if(prefix) delete [] prefix;
}

/******************************************************************************
 * PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* DiagLogProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    const char* diagq_name = StringLib::checkNullStr(argv[0]);
    const char* prefix     = StringLib::checkNullStr(argv[1]);
    int         pcenum     = NOT_PCE;

    if(argc > 2)
    {
        pcenum = (int)strtol(argv[2], NULL, 0);
        if(pcenum < 1 || pcenum > NUM_PCES)
        {
            mlog(CRITICAL, "Invalid PCE specified: %d, must be between 1 and %d\n", pcenum, NUM_PCES);
            return NULL;
        }
        else
        {
            // NOTE - pcenum currently not used but is provided for future use
        }
    }

    if(diagq_name == NULL)
    {
        mlog(CRITICAL, "Diagnostic queue cannot be null!\n");
        return NULL;
    }

    return new DiagLogProcessorModule(cmd_proc, name, diagq_name, prefix);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parse Message Log Packets
 *
 *   Notes: HS Log Message
 *----------------------------------------------------------------------------*/
bool DiagLogProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    char diagmsg[DIAG_LOG_STR_SIZE + 2];
    int msgindex = 0;
    int msgsize = 0;

    /* Populate Prefix */
    if(prefix)
    {
        msgindex = strnlen(prefix, DIAG_LOG_STR_SIZE);
        if(msgindex >= DIAG_LOG_STR_SIZE || msgindex < 0) return false;
        StringLib::copy(diagmsg, prefix, DIAG_LOG_STR_SIZE);
    }

    /* Process Packet */
    int numsegs = segments.length();
    for(int p = 0; p < numsegs; p++)
    {
        unsigned char* pktbuf = segments[p]->getBuffer();

        /* Copy Out Log Message */
        int diagmsg_len = strnlen((const char*)&pktbuf[DIAG_LOG_START], DIAG_LOG_STR_SIZE - (DIAG_LOG_START + msgindex));
        LocalLib::copy(&diagmsg[msgindex], &pktbuf[DIAG_LOG_START], diagmsg_len);
        msgsize = diagmsg_len + msgindex;

        /* Decide on New Line */
        if(diagmsg_len < (DIAG_LOG_STR_SIZE - DIAG_LOG_START))
        {
            diagmsg[msgsize] = '\n'; // insert new line for readability
            diagmsg[msgsize + 1] = '\0'; // gaurantee null termination
            msgsize += 2;
        }
        else
        {
            diagmsg[msgsize] = '\0'; // gaurantee null termination
            msgsize += 1;
        }

        /* Post Log Message */
        int status = diagQ->postCopy(diagmsg, msgsize, SYS_TIMEOUT);
        if(status <= 0)
        {
            mlog(WARNING, "Failed to post diagnostic log message: %s\n", diagmsg);
        }
    }

    return true;
}
