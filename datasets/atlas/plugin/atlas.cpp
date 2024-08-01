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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "ccsds.h"
#include "legacy.h"
#include "atlas.h"

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initatlas (void)
{
    extern CommandProcessor* cmdProc;

    /* Register SigView Handlers */
    cmdProc->registerHandler("ATLAS_FILE_WRITER",        AtlasFileWriter::createObject,             -3,  "<format: SCI_PKT, SCI_CH, SCI_TX, HISTO, CCSDS_STAT, CCSDS_INFO, META, CHANNEL, ACVPT, TIMEDIAG, TIMESTAT> <file prefix including path> <input stream>");
    cmdProc->registerHandler("ITOS_RECORD_PARSER",       ItosRecordParser::createObject,             0,  "", true);
    cmdProc->registerHandler("TIME_TAG_PROCESSOR",       TimeTagProcessorModule::createObject,       2,  "<histogram stream> <pce: 1,2,3>", true);
    cmdProc->registerHandler("ALTIMETRY_PROCESSOR",      AltimetryProcessorModule::createObject,     3,  "<histogram type: SAL, WAL, SAM, WAM, ATM> <histogram stream> <pce: 1,2,3>", true);
    cmdProc->registerHandler("MAJOR_FRAME_PROCESSOR",    MajorFrameProcessorModule::createObject,    0,  "", true);
    cmdProc->registerHandler("TIME_PROCESSOR",           TimeProcessorModule::createObject,          0,  "", true);
    cmdProc->registerHandler("LASER_PROCESSOR",          LaserProcessorModule::createObject,         0,  "", true);
    cmdProc->registerHandler("CMD_ECHO_PROCESSOR",       CmdEchoProcessorModule::createObject,      -1,  "<echo stream> <itos record parser: NULL if not specified> [<pce: 1,2,3>]", true);
    cmdProc->registerHandler("DIAG_LOG_PROCESSOR",       DiagLogProcessorModule::createObject,      -1,  "<diagnostic log stream> [<pce: 1,2,3>]", true);
    cmdProc->registerHandler("HSTVS_SIMULATOR",          HstvsSimulator::createObject,               1,  "<histogram stream>");

    /* Indicate Presence of Package */
    LuaEngine::indicate("atlas", BINID);

    /* Display Status */
    print2term("atlas plugin initialized (%s)\n", BINID);
}

void deinitatlas (void)
{
}
}
