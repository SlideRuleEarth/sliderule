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
        mlog(CRITICAL, "Must supply queue when creating publish processor module");
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
                mlog(ERROR, "Incorrect CCSDS packet length detected in %s, dropping packet (APID = x%04X, SEQ = %d, LEN = %d)", getName(), apid, seq, len);
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
                    mlog(ERROR, "Command checksum mismatch detected in %s, dropping packet (APID = x%04X, FC = %d, LEN = %d)", getName(), apid, fc, len);
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
                        memcpy(&pktbuf[bufindex], segments[copyindex]->getBuffer(), copy_len);
                        bufindex += copy_len;
                        copyindex++;
                    }

                    /* Post Packet Buffer */
                    int status = pubQ->postRef(pktbuf, bufsize, SYS_TIMEOUT);
                    if(status <= 0)
                    {
                        mlog(WARNING, "Failed to post packet in %s: %d", getName(), status);
                        delete [] pktbuf;
                        success = false;
                    }
                }
                else
                {
                    mlog(ERROR, "Dropping segments in %s due to invalid segmentation", getName());
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
                    mlog(WARNING, "Failed to post packet in %s: %d", getName(), status);
                    success = false;
                }
            }
            else
            {
                mlog(WARNING, "Header strip size exceeds length of packet in %s, dropping packet (APID = x%04X, LEN = %d)", getName(), apid, len);
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
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s", argv[0]);
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
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s", argv[0]);
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
        mlog(CRITICAL, "Invalid boolean parameter passed to command: %s", argv[0]);
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
        mlog(CRITICAL, "Invalid unsigned long parameter passed to command: %s", argv[0]);
        return -1;
    }
    else if(bytes > CCSDS_MAX_SPACE_PACKET_SIZE)
    {
        mlog(CRITICAL, "Invalid number of bytes to strip: %lu", bytes);
        return -1;
    }
    else
    {
        stripHeaderBytes = bytes;
    }

    return 0;
}
