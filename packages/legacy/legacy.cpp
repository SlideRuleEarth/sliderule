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

#include "legacy.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * CCSDS CLASS
 ******************************************************************************/

struct CCSDS: public CommandableObject
{
    static const char* NAME;
    static const int DEFAULT_MAX_FIELDS = 256;
    CCSDS(CommandProcessor* cmd_proc): CommandableObject(cmd_proc, NAME, NAME)
    {
        registerCommand("DEFINE_TELEMETRY", (cmdFunc_t)&CCSDS::defineTelemetryCmd,     5,   "<record type> <id field> <APID> <record size> <max fields>");
        registerCommand("DEFINE_COMMAND",   (cmdFunc_t)&CCSDS::defineCommandCmd,       6,   "<record type> <id field> <APID> <FC> <record size> <max fields>");
    }
    int defineTelemetryCmd(int argc, char argv[][CommandProcessor::MAX_CMD_SIZE]);
    int defineCommandCmd(int argc, char argv[][CommandProcessor::MAX_CMD_SIZE]);
};

/******************************************************************************
 * LOCAL DATA
 ******************************************************************************/

CommandProcessor* cmdProc = NULL;
CCSDS* ccsdsCmds = NULL;
const char* CCSDS::NAME = "CCSDS";

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * CCSDS - defineTelemetryCmd
 *----------------------------------------------------------------------------*/
int CCSDS::defineTelemetryCmd (int argc, char argv[][CommandProcessor::MAX_CMD_SIZE])
{
    (void)argc;

    const char* rec_type = StringLib::checkNullStr(argv[0]);
    const char* id_field = StringLib::checkNullStr(argv[1]);
    const char* apid_str = argv[2];
    const char* size_str = argv[3];
    const char* max_str = argv[4];

    /* Check Rec Type */
    if(rec_type == NULL)
    {
        mlog(CRITICAL, "Must supply a record type");
        return -1;
    }

    /* Get Apid */
    long apid = 0;
    if(!StringLib::str2long(apid_str, &apid))
    {
        mlog(CRITICAL, "Invalid APID supplied: %s", apid_str);
        return -1;
    }
    else if(apid < 0 || apid > CCSDS_NUM_APIDS)
    {
        mlog(CRITICAL, "Invalid APID supplied: %ld", apid);
        return -1;
    }

    /* Get Size */
    long size = 0;
    if(!StringLib::str2long(size_str, &size))
    {
        mlog(CRITICAL, "Invalid size supplied: %s", size_str);
        return -1;
    }
    else if(size <= 0)
    {
        mlog(CRITICAL, "Invalid size supplied: %ld", size);
        return -1;
    }

    /* Get Max Fields */
    long max_fields = 0;
    if(!StringLib::str2long(max_str, &max_fields))
    {
        mlog(CRITICAL, "Invalid max fields supplied: %s", max_str);
        return -1;
    }
    else if(max_fields < 0)
    {
        mlog(CRITICAL, "Invalid max field value supplied: %ld", max_fields);
        return -1;
    }
    else if(max_fields == 0)
    {
         max_fields = DEFAULT_MAX_FIELDS;
    }

    /* Define Record */
    RecordObject::recordDefErr_t status = CcsdsRecord::defineTelemetry(rec_type, id_field, (uint16_t)apid, size, NULL, 0, max_fields);
    if(status != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define telemetry packet %s: %d", rec_type, (int)status);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * CCSDS - defineCommandCmd
 *----------------------------------------------------------------------------*/
int CCSDS::defineCommandCmd (int argc, char argv[][CommandProcessor::MAX_CMD_SIZE])
{
    (void)argc;

    const char* rec_type = StringLib::checkNullStr(argv[0]);
    const char* id_field = StringLib::checkNullStr(argv[1]);
    const char* apid_str = argv[2];
    const char* fc_str = argv[3];
    const char* size_str = argv[4];
    const char* max_str = argv[5];

    /* Check Rec Type */
    if(rec_type == NULL)
    {
        mlog(CRITICAL, "Must supply a record type");
        return -1;
    }

    /* Get Apid */
    long apid = 0;
    if(!StringLib::str2long(apid_str, &apid))
    {
        mlog(CRITICAL, "Invalid APID supplied: %s", apid_str);
        return -1;
    }
    else if(apid < 0 || apid > CCSDS_NUM_APIDS)
    {
        mlog(CRITICAL, "Invalid APID supplied: %ld", apid);
        return -1;
    }

    /* Get Function Code */
    long fc = 0;
    if(!StringLib::str2long(fc_str, &fc))
    {
        mlog(CRITICAL, "Invalid function code supplied: %s", fc_str);
        return -1;
    }
    else if((fc & 0x7F) != fc)
    {
        mlog(CRITICAL, "Invalid function code supplied: %ld", fc);
        return -1;
    }

    /* Get Size */
    long size = 0;
    if(!StringLib::str2long(size_str, &size))
    {
        mlog(CRITICAL, "Invalid size supplied: %s", size_str);
        return -1;
    }
    else if(size <= 0)
    {
        mlog(CRITICAL, "Invalid size supplied: %ld", size);
        return -1;
    }

    /* Get Max Fields */
    long max_fields = 0;
    if(!StringLib::str2long(max_str, &max_fields))
    {
        mlog(CRITICAL, "Invalid max fields supplied: %s", max_str);
        return -1;
    }
    else if(max_fields < 0)
    {
        mlog(CRITICAL, "Invalid max field value supplied: %ld", max_fields);
        return -1;
    }
    else if(max_fields == 0)
    {
         max_fields = DEFAULT_MAX_FIELDS;
    }

    /* Define Record */
    RecordObject::recordDefErr_t status = CcsdsRecord::defineCommand(rec_type, id_field, (uint16_t)apid, (uint8_t)fc, size, NULL, 0, max_fields);
    if(status != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define command packet %s: %d", rec_type, (int)status);
        return -1;
    }

    return 0;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initlegacy
 *----------------------------------------------------------------------------*/
void initlegacy (void)
{
    /* Create Default Objects */
    cmdProc = new CommandProcessor(CMDQ);
    ccsdsCmds = new CCSDS(cmdProc);

     /* Register Add On Commands */
    cmdProc->registerObject(CCSDS::NAME, ccsdsCmds);

    /* Register Default Handlers */
    cmdProc->registerHandler("CCSDS_PACKET_PROCESSOR",      CcsdsPacketProcessor::createObject,             2,  "<input stream> <number of workers>");
    cmdProc->registerHandler("CCSDS_FILE_WRITER",           CcsdsFileWriter::createObject,                 -3,  "<RAW_BINARY|RAW_ASCII|TEXT> <prefix> <input stream> [<max file size>]");
    cmdProc->registerHandler("CCSDS_FRAME_STRIPPER",        CcsdsFrameStripper::createObject,               5,  "<in stream> <out stream> <Sync Marker> <Leading Strip Size> <Fixed Frame Size>");
    cmdProc->registerHandler("CCSDS_RECORD_FILE_WRITER",    CcsdsRecordFileWriter::createObject,           -2,  "<prefix> <input stream> [[<max file size>] [<field name> ...]]");
    cmdProc->registerHandler("CFS_INTERFACE",               CfsInterface::createObject,                     6,  "<tlm stream> <cmd stream> <tlm ip addr> <tlm port> <cmd ip addr> <cmd port>");
    cmdProc->registerHandler("COSMOS_INTERFACE",            CosmosInterface::createObject,                 -6,  "<tlm stream> <cmd stream> <tlm ip addr> <tlm port> <cmd ip addr> <cmd port> [<max connections>]");
    cmdProc->registerHandler("LUA_INTERPRETER",             LuaInterpreter::createUnsafeObject,            -1,  "<input stream: msgq mode | STDIN: stdin mode | FILE: file mode> [additional lua arguments]");
    cmdProc->registerHandler("LUA_SAFE_INTERPRETER",        LuaInterpreter::createSafeObject,              -1,  "<input stream: msgq mode | STDIN: stdin mode | FILE: file mode> [additional lua arguments]");
    cmdProc->registerHandler("PUBLISHER_PROCESSOR",         CcsdsPublisherProcessorModule::createObject,    1,  "<output stream>", true);
    cmdProc->registerHandler("UT_DICTIONARY",               UT_Dictionary::createObject,                    0,  "");
    cmdProc->registerHandler("UT_LIST",                     UT_List::createObject,                          0,  "");
    cmdProc->registerHandler("UT_MSGQ",                     UT_MsgQ::createObject,                          0,  "");
    cmdProc->registerHandler("UT_TABLE"  ,                  UT_Table::createObject,                         0,  "");
    cmdProc->registerHandler("UT_TIMELIB",                  UT_TimeLib::createObject,                       0,  "");

    /* Add Lua Extension */
    LuaLibraryCmd::lcmd_init(cmdProc);
    LuaEngine::extend(LuaLibraryCmd::LUA_CMDLIBNAME, LuaLibraryCmd::luaopen_cmdlib);

    /* Indicate Presence of Package */
    LuaEngine::indicate("legacy", LIBID);

    /* Print Status */
    print2term("legacy package initialized (%s)\n", LIBID);
}

/*----------------------------------------------------------------------------
 * deinitlegacy
 *----------------------------------------------------------------------------*/
void deinitlegacy (void)
{
    delete cmdProc;
}
