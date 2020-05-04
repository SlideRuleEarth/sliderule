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

#include "CcsdsPublisherProcessorModule.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* CcsdsPublisherProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    const char* pubq_name = StringLib::checkNullStr(argv[0]);

    if(pubq_name == NULL)
    {
        mlog(CRITICAL, "Must supply queue when creating publish processor module\n");
        return NULL;
    }

    return new CcsdsPublisherProcessorModule(cmd_proc, name, pubq_name);
}

/*----------------------------------------------------------------------------
 * freePkt
 *----------------------------------------------------------------------------*/
void CcsdsPublisherProcessorModule::freePkt(void* obj, void* parm)
{
    (void)parm;
    unsigned char* pkt = (unsigned char*)obj;
    delete [] pkt;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CcsdsPublisherProcessorModule::CcsdsPublisherProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, const char* pubq_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    assert(pubq_name);
    pubQ = new Publisher(pubq_name, freePkt);

    concatSegments = false;
    checkLength = false;
    checkChecksum = false;

    stripHeaderBytes = 0;

    registerCommand("CONCAT_SEGMENTS",  (cmdFunc_t)&CcsdsPublisherProcessorModule::concatSegmentsCmd,   1, "<ENABLE|DISABLE>");
    registerCommand("CHECK_LENGTH",     (cmdFunc_t)&CcsdsPublisherProcessorModule::checkLengthCmd,      1, "<ENABLE|DISABLE>");
    registerCommand("CHECK_CHECKSUM",   (cmdFunc_t)&CcsdsPublisherProcessorModule::checkChecksumCmd,    1, "<ENABLE|DISABLE>");
    registerCommand("STRIP_HEADER",     (cmdFunc_t)&CcsdsPublisherProcessorModule::stripHeaderCmd,      1, "<bytes>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CcsdsPublisherProcessorModule::~CcsdsPublisherProcessorModule(void)
{
    delete pubQ;
}

/*----------------------------------------------------------------------------
 * processSegments  - Chunk out sets of CCSDS Packet segments
 *
 *   Notes: If retries are enabled, then no integration should be used (i.e. numpkts
 *          should be 1).  Otherwise, part of the packets could be sent, then a failure
 *          would result in a retry on all of them.  Duplicate data could result.
 *----------------------------------------------------------------------------*/
bool CcsdsPublisherProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    bool success = true;
    int segindex = 0;
    int bufsize = 0;
    int copyindex = segindex;
    int prevseg = CcsdsSpacePacket::SEG_NONE;

    while(segindex < segments.length())
    {
        CcsdsSpacePacket* pkt = segments[segindex++];
        int apid = pkt->getAPID();
        int len = pkt->getLEN();
        int seq = pkt->getSEQ();
        int seg = pkt->getSEQFLG();

        /* Check Packet Length */
        if(checkLength)
        {
            if(!pkt->isFull())
            {
                mlog(ERROR, "Incorrect CCSDS packet length detected in %s, dropping packet (APID = x%04X, SEQ = %d, LEN = %d)\n", getName(), apid, seq, len);
                continue;
            }
        }

        /* Check Packet Checksum (if command) */
        if(checkChecksum)
        {
            if(pkt->isCMD())
            {
                if(!pkt->validChecksum())
                {
                    int fc = pkt->getFunctionCode();
                    mlog(ERROR, "Command checksum mismatch detected in %s, dropping packet (APID = x%04X, FC = %d, LEN = %d)\n", getName(), apid, fc, len);
                    continue;
                }
            }
        }

        /* Publish Packet(s) */
        if(concatSegments)
        {
            /* Update Buffer Size */
            bufsize += MAX(len - stripHeaderBytes, 0);

            /* Perform Concatenation at Segment Boundaries */
            if(seg == CcsdsSpacePacket::SEG_NONE || seg == CcsdsSpacePacket::SEG_STOP)
            {
                /* Check for Valid Segment Transition */
                if((seg == CcsdsSpacePacket::SEG_NONE && prevseg == CcsdsSpacePacket::SEG_NONE) ||
                   (seg == CcsdsSpacePacket::SEG_STOP && prevseg == CcsdsSpacePacket::SEG_START))
                {
                    /* Populate Packet Buffer with Segments */
                    unsigned char* pktbuf = new unsigned char[bufsize];
                    int bufindex = 0;
                    while(copyindex < segindex)
                    {
                        int copy_len = MAX(segments[copyindex]->getLEN() - stripHeaderBytes, 0);
                        LocalLib::copy(&pktbuf[bufindex], segments[copyindex]->getBuffer(), copy_len);
                        bufindex += copy_len;
                        copyindex++;
                    }

                    /* Post Packet Buffer */
                    int status = pubQ->postRef(pktbuf, bufsize, SYS_TIMEOUT);
                    if(status <= 0)
                    {
                        mlog(WARNING, "Failed to post packet in %s: %d\n", getName(), status);
                        delete [] pktbuf;
                        success = false;
                    }
                }
                else
                {
                    mlog(ERROR, "Dropping segments in %s due to invalid segmentation\n", getName());
                }

                /* Reset Buffer Size and Copy Index */
                bufsize = 0;
                copyindex = segindex;
            }

            /* Set Previous Segment */
            if(seg != CcsdsSpacePacket::SEG_CONTINUE) prevseg = seg;
        }
        else
        {
            if(stripHeaderBytes < len)
            {
                /* Post Packet Buffer */
                unsigned char* pktbuf = pkt->getBuffer();
                int status = pubQ->postCopy(&pktbuf[stripHeaderBytes], len - stripHeaderBytes, SYS_TIMEOUT);
                if(status <= 0)
                {
                    mlog(WARNING, "Failed to post packet in %s: %d\n", getName(), status);
                    success = false;
                }
            }
            else
            {
                mlog(WARNING, "Header strip size exceeds length of packet in %s, dropping packet (APID = x%04X, LEN = %d)\n", getName(), apid, len);
            }
        }
    }

    return success;
}

/*----------------------------------------------------------------------------
 * concatSegmentsCmd  -
 *----------------------------------------------------------------------------*/
int CcsdsPublisherProcessorModule::concatSegmentsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable))
    {
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s\n", argv[0]);
        return -1;
    }
    else
    {
        concatSegments = enable;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * checkLengthCmd  -
 *----------------------------------------------------------------------------*/
int CcsdsPublisherProcessorModule::checkLengthCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable))
    {
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s\n", argv[0]);
        return -1;
    }
    else
    {
        checkLength = enable;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * checkChecksumCmd  -
 *----------------------------------------------------------------------------*/
int CcsdsPublisherProcessorModule::checkChecksumCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable))
    {
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s\n", argv[0]);
        return -1;
    }
    else
    {
        checkChecksum = enable;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * stripHeaderCmd  -
 *----------------------------------------------------------------------------*/
int CcsdsPublisherProcessorModule::stripHeaderCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    unsigned long bytes;
    if(!StringLib::str2ulong(argv[0], &bytes))
    {
        mlog(CRITICAL, "Invalid unsigned long parameter passed to command: %s\n", argv[0]);
        return -1;
    }
    else if(bytes > CCSDS_MAX_SPACE_PACKET_SIZE)
    {
        mlog(CRITICAL, "Invalid number of bytes to strip: %lu\n", bytes);
        return -1;
    }
    else
    {
        stripHeaderBytes = bytes;
    }

    return 0;
}
