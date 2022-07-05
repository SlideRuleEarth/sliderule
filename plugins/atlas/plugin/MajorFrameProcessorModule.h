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

#ifndef __major_frame_processor_module__
#define __major_frame_processor_module__

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class MajorFrameProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_BKGND_CNTS = 8;
        static const int MAX_KEY_NAME_SIZE = 64;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            int     OnePPSCount;
            long    IMETAtOnePPS;
            long    IMETAtFirstT0;
            int     T0Counter;
            long    MajorFrameCount;
            int     BackgroundCounts[NUM_BKGND_CNTS];
            int     CalibrationValueRising;
            int     CalibrationValueFalling;
            int     LatestLeadingStartTimeTag;
            int     LatestTrailingStartTimeTag;
            int     LastSequencePacketCount;
            int     CommandCount;
            int     LastCommandOpcode;
            int     SciencePacketLogicalAddress;
            int     DuplicateTimeTagRemovalMargin;
            int     CalibrationRequestIntervalFromTDC;
            int     ScienceDataSegmentLimit;
            int     TagWaitWatchdogValue;
            int     TagWriteWatchdogValue;
            int     MajorFrameFrequency;
            int     Configuration;
            int     StrongAltimetricRangeWindowStart;
            int     StrongAltimetricRangeWindowWidth;
            int     StrongAtmosphericRangeWindowStart;
            int     StrongAtmosphericRangeWindowWidth;
            int     WeakAltimetricRangeWindowStart;
            int     WeakAltimetricRangeWindowWidth;
            int     WeakAtmosphericRangeWindowStart;
            int     WeakAtmosphericRangeWindowWidth;
            long    DebugControlReg;
            int     GeneralPurposeReg;
            int     EDACStatusBits;
            int         EDACSingleBitErrorCnt;
            bool        EDACStartTrackingFIFO_DBE;
            bool        EDACStartTagFIFO_DBE;
            bool        EDACSDRAM_B_DBE;
            bool        EDACSDRAM_A_DBE;
            bool        EDACMFNumber_DBE;
            bool        EDACEventTagFIFO_DBE;
            bool        EDACCardReadoutRAM_DBE;
            bool        EDACCardCreationRAM_DBE;
            bool        EDACBurstFIFO_DBE;
            bool        EDACPacketFIFO_B_DBE;
            bool        EDACPacketFIFO_A_DBE;
            bool        EDACStartTrackingFIFO_SBE;
            bool        EDACStartTagFIFO_SBE;
            bool        EDACSDRAM_B_SBE;
            bool        EDACSDRAM_A_SBE;
            bool        EDACMFNumber_SBE;
            bool        EDACEventTagFIFO_SBE;
            bool        EDACCardFlagRAM_SBE;
            bool        EDACCardReadoutRAM_SBE;
            bool        EDACCardCreationRAM_SBE;
            bool        EDACBurstFIFO_SBE;
            bool        EDACPacketFIFO_B_SBE;
            bool        EDACPacketFIFO_A_SBE;
            long    DFCHousekeepingStatusBits;  // THIS is overriden when a meaningful OR of TDC_StrongPath_Err, TDC_WeakPath_Err, TDC_FIFOWentFull, EventTag_FIFOWentFull, StartTag_FIFOWentFull
            bool        RangeWindowDropout_Err;
            bool        TDC_StrongPath_Err;
            bool        TDC_WeakPath_Err;
            bool        TDC_FIFOHalfFull;
            bool        TDC_FIFOEmpty;
            bool        EventTag_FIFOEmpty;
            bool        Burst_FIFOEmpty;
            bool        StartTag_FIFOEmpty;
            bool        Tracking_FIFOEmpty;
            bool        PacketizerA_FIFOEmpty;
            bool        PacketizerB_FIFOEmpty;
            bool        TDC_FIFOWentFull;
            bool        EventTag_FIFOWentFull;
            bool        Burst_FIFOWentFull;
            bool        StartTag_FIFOWentFull;
            bool        Tracking_FIFOWentFull;
            bool        PacketizerA_FIFOWentFull;
            bool        PacketizerB_FIFOWentFull;
            int         TxPulsesInMajorFrame;
            int     DFCStatusBits;
            bool        DidNotFinishTransfer_Err;
            bool        SDRAMMismatch_Err;
            bool        DidNotFinishWritingData_Err;
            bool        SpwRxEEP_Err;
            bool        SpwRxInvalidLength_Err;
            bool        SpwRxInvalidOpcode_Err;
            bool        SpwRxProtocolIDErr;
            bool        CurrentReadSDRAMBuffer;
            int     DebugStatusBits;
            bool        StartDataCollection;
            bool        CardDataNotFinished_Err;
            int         FPGAVersion;
            int         SpwLinkVersion;
            int         SpwDebugMuxOut;
            int     SpwNotReadyCounter;

        } majorFrameData_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* majorFrameDataKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        MajorFrameProcessorModule   (CommandProcessor* cmd_proc, const char* obj_name);
                        ~MajorFrameProcessorModule  (void);

        static void     buildKey                    (long mfc, char* name_buf);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        majorFrameData_t majorFrameData;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool processSegments (List<CcsdsSpacePacket*>& segments, int numpkts);
};

typedef MajorFrameProcessorModule::majorFrameData_t mfdata_t; // short cut

#endif  /* __major_frame_processor_module__ */
