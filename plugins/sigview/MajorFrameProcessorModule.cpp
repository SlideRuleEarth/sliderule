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
 INCLUDES
 ******************************************************************************/

#include "MajorFrameProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MajorFrameProcessorModule::majorFrameDataKey = "mfdata";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
MajorFrameProcessorModule::MajorFrameProcessorModule(CommandProcessor* cmd_proc, const char* obj_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    /* Initialize Parameters */
    memset(&majorFrameData, 0, sizeof(majorFrameData));

    /* Post Initial Values to Current Value Table */
    cmdProc->setCurrentValue(getName(), majorFrameDataKey, (void*)&majorFrameData, sizeof(majorFrameData));
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
MajorFrameProcessorModule::~MajorFrameProcessorModule(void)
{
}

/*----------------------------------------------------------------------------
 * buildKey  -
 *----------------------------------------------------------------------------*/
void MajorFrameProcessorModule::buildKey(long mfc, char* name_buf)
{
    snprintf(name_buf, MAX_KEY_NAME_SIZE, "%s.%ld", majorFrameDataKey, mfc & 0xFF);
}

/******************************************************************************
 * PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* MajorFrameProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    return new MajorFrameProcessorModule(cmd_proc, name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * parse_majorframe_pkt  - Parser for Major Frame Packet
 *
 *   Notes: MFP
 *----------------------------------------------------------------------------*/
bool MajorFrameProcessorModule::processSegments (List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    int numsegs = segments.length();
    if(numsegs < 1) return false;

    /* Process Segment */
    int             p       = numsegs - 1;
    unsigned char*  pktbuf  = segments[p]->getBuffer();
    long            mfc     = parseInt(pktbuf + 30, 4);

    /* Populate Major Frame Data */
    majorFrameData.OnePPSCount                            = parseInt(pktbuf + 12, 4);
    majorFrameData.IMETAtOnePPS                           = parseInt(pktbuf + 16, 6);
    majorFrameData.IMETAtFirstT0                          = parseInt(pktbuf + 22, 6);
    majorFrameData.T0Counter                              = parseInt(pktbuf + 28, 2);
    majorFrameData.MajorFrameCount                        = parseInt(pktbuf + 30, 4);
    majorFrameData.BackgroundCounts[0]                    = parseInt(pktbuf + 34, 2);
    majorFrameData.BackgroundCounts[1]                    = parseInt(pktbuf + 36, 2);
    majorFrameData.BackgroundCounts[2]                    = parseInt(pktbuf + 38, 2);
    majorFrameData.BackgroundCounts[3]                    = parseInt(pktbuf + 40, 2);
    majorFrameData.BackgroundCounts[4]                    = parseInt(pktbuf + 42, 2);
    majorFrameData.BackgroundCounts[5]                    = parseInt(pktbuf + 44, 2);
    majorFrameData.BackgroundCounts[6]                    = parseInt(pktbuf + 46, 2);
    majorFrameData.BackgroundCounts[7]                    = parseInt(pktbuf + 48, 2);
    majorFrameData.CalibrationValueRising                 = parseInt(pktbuf + 50, 2);
    majorFrameData.CalibrationValueFalling                = parseInt(pktbuf + 52, 2);
    majorFrameData.LatestLeadingStartTimeTag              = parseInt(pktbuf + 54, 3);
    majorFrameData.LatestTrailingStartTimeTag             = parseInt(pktbuf + 57, 3);
    majorFrameData.LastSequencePacketCount                = parseInt(pktbuf + 60, 2);
    majorFrameData.CommandCount                           = parseInt(pktbuf + 62, 2);
    majorFrameData.LastCommandOpcode                      = parseInt(pktbuf + 64, 1);
    majorFrameData.SciencePacketLogicalAddress            = parseInt(pktbuf + 65, 1);
    majorFrameData.DuplicateTimeTagRemovalMargin          = parseInt(pktbuf + 66, 2);
    majorFrameData.CalibrationRequestIntervalFromTDC      = parseInt(pktbuf + 68, 1);
    majorFrameData.ScienceDataSegmentLimit                = parseInt(pktbuf + 69, 2);
    majorFrameData.TagWaitWatchdogValue                   = parseInt(pktbuf + 71, 1);
    majorFrameData.TagWriteWatchdogValue                  = parseInt(pktbuf + 72, 2);
    majorFrameData.MajorFrameFrequency                    = parseInt(pktbuf + 74, 1);
    majorFrameData.Configuration                          = parseInt(pktbuf + 75, 1);
    majorFrameData.StrongAltimetricRangeWindowStart       = parseInt(pktbuf + 76, 3);
    majorFrameData.StrongAltimetricRangeWindowWidth       = parseInt(pktbuf + 79, 2);
    majorFrameData.StrongAtmosphericRangeWindowStart      = parseInt(pktbuf + 81, 3);
    majorFrameData.StrongAtmosphericRangeWindowWidth      = parseInt(pktbuf + 84, 2);
    majorFrameData.WeakAltimetricRangeWindowStart         = parseInt(pktbuf + 86, 3);
    majorFrameData.WeakAltimetricRangeWindowWidth         = parseInt(pktbuf + 89, 2);
    majorFrameData.WeakAtmosphericRangeWindowStart        = parseInt(pktbuf + 91, 3);
    majorFrameData.WeakAtmosphericRangeWindowWidth        = parseInt(pktbuf + 94, 2);
    majorFrameData.DebugControlReg                        = parseInt(pktbuf + 96, 4);
    majorFrameData.GeneralPurposeReg                      = parseInt(pktbuf + 100, 2);
    majorFrameData.EDACStatusBits                         = parseInt(pktbuf + 102, 4);
        majorFrameData.EDACSingleBitErrorCnt              = (majorFrameData.EDACStatusBits & 0xFF000000) >> 24;
        majorFrameData.EDACStartTrackingFIFO_DBE          = ((majorFrameData.EDACStatusBits & 0x00400000) >> 22) == 0 ? false : true;
        majorFrameData.EDACStartTagFIFO_DBE               = ((majorFrameData.EDACStatusBits & 0x00200000) >> 21) == 0 ? false : true;
        majorFrameData.EDACSDRAM_B_DBE                    = ((majorFrameData.EDACStatusBits & 0x00100000) >> 20) == 0 ? false : true;
        majorFrameData.EDACSDRAM_A_DBE                    = ((majorFrameData.EDACStatusBits & 0x00080000) >> 19) == 0 ? false : true;
        majorFrameData.EDACMFNumber_DBE                   = ((majorFrameData.EDACStatusBits & 0x00040000) >> 18) == 0 ? false : true;
        majorFrameData.EDACEventTagFIFO_DBE               = ((majorFrameData.EDACStatusBits & 0x00020000) >> 17) == 0 ? false : true;
        majorFrameData.EDACCardReadoutRAM_DBE             = ((majorFrameData.EDACStatusBits & 0x00010000) >> 16) == 0 ? false : true;
        majorFrameData.EDACCardCreationRAM_DBE            = ((majorFrameData.EDACStatusBits & 0x00008000) >> 15) == 0 ? false : true;
        majorFrameData.EDACBurstFIFO_DBE                  = ((majorFrameData.EDACStatusBits & 0x00004000) >> 14) == 0 ? false : true;
        majorFrameData.EDACPacketFIFO_B_DBE               = ((majorFrameData.EDACStatusBits & 0x00002000) >> 13) == 0 ? false : true;
        majorFrameData.EDACPacketFIFO_A_DBE               = ((majorFrameData.EDACStatusBits & 0x00001000) >> 12) == 0 ? false : true;
        majorFrameData.EDACStartTrackingFIFO_SBE          = ((majorFrameData.EDACStatusBits & 0x00000800) >> 11) == 0 ? false : true;
        majorFrameData.EDACStartTagFIFO_SBE               = ((majorFrameData.EDACStatusBits & 0x00000400) >> 10) == 0 ? false : true;
        majorFrameData.EDACSDRAM_B_SBE                    = ((majorFrameData.EDACStatusBits & 0x00000200) >> 9) == 0 ? false : true;
        majorFrameData.EDACSDRAM_A_SBE                    = ((majorFrameData.EDACStatusBits & 0x00000100) >> 8) == 0 ? false : true;
        majorFrameData.EDACMFNumber_SBE                   = ((majorFrameData.EDACStatusBits & 0x00000080) >> 7) == 0 ? false : true;
        majorFrameData.EDACEventTagFIFO_SBE               = ((majorFrameData.EDACStatusBits & 0x00000040) >> 6) == 0 ? false : true;
        majorFrameData.EDACCardFlagRAM_SBE                = ((majorFrameData.EDACStatusBits & 0x00000020) >> 5) == 0 ? false : true;
        majorFrameData.EDACCardReadoutRAM_SBE             = ((majorFrameData.EDACStatusBits & 0x00000010) >> 4) == 0 ? false : true;
        majorFrameData.EDACCardCreationRAM_SBE            = ((majorFrameData.EDACStatusBits & 0x00000008) >> 3) == 0 ? false : true;
        majorFrameData.EDACBurstFIFO_SBE                  = ((majorFrameData.EDACStatusBits & 0x00000004) >> 2) == 0 ? false : true;
        majorFrameData.EDACPacketFIFO_B_SBE               = ((majorFrameData.EDACStatusBits & 0x00000002) >> 1) == 0 ? false : true;
        majorFrameData.EDACPacketFIFO_A_SBE               = ((majorFrameData.EDACStatusBits & 0x00000001) >> 0) == 0 ? false : true;
    majorFrameData.DFCHousekeepingStatusBits              = parseInt(pktbuf + 106, 4);
        majorFrameData.RangeWindowDropout_Err             = ((majorFrameData.DFCHousekeepingStatusBits & 0x00040000) >> 18) == 0 ? false : true;
        majorFrameData.TDC_StrongPath_Err                 = ((majorFrameData.DFCHousekeepingStatusBits & 0x00020000) >> 17) == 0 ? false : true;
        majorFrameData.TDC_WeakPath_Err                   = ((majorFrameData.DFCHousekeepingStatusBits & 0x00010000) >> 16) == 0 ? false : true;
        majorFrameData.TDC_FIFOHalfFull                   = ((majorFrameData.DFCHousekeepingStatusBits & 0x00008000) >> 15) == 0 ? false : true;
        majorFrameData.TDC_FIFOEmpty                      = ((majorFrameData.DFCHousekeepingStatusBits & 0x00004000) >> 14) == 0 ? false : true;
        majorFrameData.EventTag_FIFOEmpty                 = ((majorFrameData.DFCHousekeepingStatusBits & 0x00002000) >> 13) == 0 ? false : true;
        majorFrameData.Burst_FIFOEmpty                    = ((majorFrameData.DFCHousekeepingStatusBits & 0x00001000) >> 12) == 0 ? false : true;
        majorFrameData.StartTag_FIFOEmpty                 = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000400) >> 10) == 0 ? false : true; // skipped unused bit
        majorFrameData.Tracking_FIFOEmpty                 = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000200) >> 9) == 0 ? false : true;
        majorFrameData.PacketizerA_FIFOEmpty              = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000100) >> 8) == 0 ? false : true;
        majorFrameData.PacketizerB_FIFOEmpty              = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000080) >> 7) == 0 ? false : true;
        majorFrameData.TDC_FIFOWentFull                   = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000040) >> 6) == 0 ? false : true;
        majorFrameData.EventTag_FIFOWentFull              = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000020) >> 5) == 0 ? false : true;
        majorFrameData.Burst_FIFOWentFull                 = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000010) >> 4) == 0 ? false : true;
        majorFrameData.StartTag_FIFOWentFull              = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000008) >> 3) == 0 ? false : true;
        majorFrameData.Tracking_FIFOWentFull              = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000004) >> 2) == 0 ? false : true;
        majorFrameData.PacketizerA_FIFOWentFull           = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000002) >> 1) == 0 ? false : true;
        majorFrameData.PacketizerB_FIFOWentFull           = ((majorFrameData.DFCHousekeepingStatusBits & 0x00000001) >> 0) == 0 ? false : true;
    majorFrameData.TxPulsesInMajorFrame                   = parseInt(pktbuf + 110, 1);
    majorFrameData.DFCStatusBits                          = parseInt(pktbuf + 111, 1);
        majorFrameData.DidNotFinishTransfer_Err           = ((majorFrameData.DFCStatusBits & 0x80) >> 7) == 0 ? false : true;
        majorFrameData.SDRAMMismatch_Err                  = ((majorFrameData.DFCStatusBits & 0x40) >> 6) == 0 ? false : true;
        majorFrameData.DidNotFinishWritingData_Err        = ((majorFrameData.DFCStatusBits & 0x20) >> 5) == 0 ? false : true;
        majorFrameData.SpwRxEEP_Err                       = ((majorFrameData.DFCStatusBits & 0x10) >> 4) == 0 ? false : true;
        majorFrameData.SpwRxInvalidLength_Err             = ((majorFrameData.DFCStatusBits & 0x08) >> 3) == 0 ? false : true;
        majorFrameData.SpwRxInvalidOpcode_Err             = ((majorFrameData.DFCStatusBits & 0x04) >> 2) == 0 ? false : true;
        majorFrameData.SpwRxProtocolIDErr                 = ((majorFrameData.DFCStatusBits & 0x02) >> 1) == 0 ? false : true;
        majorFrameData.CurrentReadSDRAMBuffer             = ((majorFrameData.DFCStatusBits & 0x01) >> 0) == 0 ? false : true;
    majorFrameData.DebugStatusBits                        = parseInt(pktbuf + 112, 3);
        majorFrameData.StartDataCollection                = ((majorFrameData.DebugStatusBits & 0x800000) >> 23) == 0 ? false : true;
        majorFrameData.CardDataNotFinished_Err            = ((majorFrameData.DebugStatusBits & 0x400000) >> 22) == 0 ? false : true;
        majorFrameData.FPGAVersion                        = (majorFrameData.DebugStatusBits & 0x3F0000) >> 16;
        majorFrameData.SpwLinkVersion                     = (majorFrameData.DebugStatusBits & 0x00FF00) >> 8;
        majorFrameData.SpwDebugMuxOut                     = (majorFrameData.DebugStatusBits & 0x0000FF) >> 0;
    majorFrameData.SpwNotReadyCounter                     = parseInt(pktbuf + 115, 2);

    /* OVERRIDE DFC HOUSEKEEPING STATUS BIT WITH MEANINGFUL OR */
    majorFrameData.DFCHousekeepingStatusBits = majorFrameData.TDC_StrongPath_Err ||
                                                  majorFrameData.TDC_WeakPath_Err ||
                                                  majorFrameData.TDC_FIFOWentFull ||
                                                  majorFrameData.EventTag_FIFOWentFull ||
                                                  majorFrameData.StartTag_FIFOWentFull;

    /* Post to Current Value Table */
    char keyname[MAX_KEY_NAME_SIZE];
    buildKey(mfc, keyname);
    cmdProc->setCurrentValue(getName(), keyname, &majorFrameData, sizeof(majorFrameData_t));

    return true;
}
