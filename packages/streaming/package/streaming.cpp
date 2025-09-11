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


#include "AncillaryFields.h"
#include "CaptureDispatch.h"
#include "ClusterSocket.h"
#include "ContainerRecord.h"
#include "CsvDispatch.h"
#include "DispatchObject.h"
#include "DeviceIO.h"
#include "DeviceObject.h"
#include "DeviceReader.h"
#include "DeviceWriter.h"
#include "File.h"
#include "HttpClient.h"
#include "LimitDispatch.h"
#include "LimitRecord.h"
#include "MetricDispatch.h"
#include "MetricRecord.h"
#include "MsgBridge.h"
#include "MsgProcessor.h"
#include "PublisherDispatch.h"
#include "RecordDispatcher.h"
#include "ReportDispatch.h"
#include "TcpSocket.h"
#include "Uart.h"
#include "UdpSocket.h"
#include "OsApi.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_STREAMING_LIBNAME  "streaming"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * stream_open
 *----------------------------------------------------------------------------*/
static int stream_open (lua_State *L)
{
    static const struct luaL_Reg streaming_functions[] = {
        {"cluster",         ClusterSocket::luaCreate},
        {"file",            File::luaCreate},
        {"tcp",             TcpSocket::luaCreate},
        {"uart",            Uart::luaCreate},
        {"udp",             UdpSocket::luaCreate},
        {"reader",          DeviceReader::luaCreate},
        {"writer",          DeviceWriter::luaCreate},
        {"http",            HttpClient::luaCreate},
        {"dispatcher",      RecordDispatcher::luaCreate},
        {"capture",         CaptureDispatch::luaCreate},
        {"limit",           LimitDispatch::luaCreate},
        {"metric",          MetricDispatch::luaCreate},
        {"publish",         PublisherDispatch::luaCreate},
        {"report",          ReportDispatch::luaCreate},
        {"csv",             CsvDispatch::luaCreate},
        {"bridge",          MsgBridge::luaCreate},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, streaming_functions);

    /* Set Globals */
    LuaEngine::setAttrInt   (L, "READER",                   DeviceObject::READER);
    LuaEngine::setAttrInt   (L, "WRITER",                   DeviceObject::WRITER);
    LuaEngine::setAttrInt   (L, "DUPLEX",                   DeviceObject::DUPLEX);
    LuaEngine::setAttrBool  (L, "SERVER",                   true);
    LuaEngine::setAttrBool  (L, "CLIENT",                   false);
    LuaEngine::setAttrInt   (L, "DIE_ON_DISCONNECT",        true);
    LuaEngine::setAttrInt   (L, "PERSISTENT",               false);
    LuaEngine::setAttrInt   (L, "BLOCK",                    true);
    LuaEngine::setAttrInt   (L, "QUEUE",                    ClusterSocket::QUEUE);
    LuaEngine::setAttrInt   (L, "BUS",                      ClusterSocket::BUS);
    LuaEngine::setAttrInt   (L, "BINARY",                   File::BINARY);
    LuaEngine::setAttrInt   (L, "ASCII",                    File::ASCII);
    LuaEngine::setAttrInt   (L, "TEXT",                     File::TEXT);
    LuaEngine::setAttrInt   (L, "FIFO",                     File::FIFO);
    LuaEngine::setAttrInt   (L, "FLUSHED",                  File::FLUSHED);
    LuaEngine::setAttrInt   (L, "CACHED",                   File::CACHED);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 *  initstreaming
 *
 *  initialize streaming package
 *----------------------------------------------------------------------------*/
void initstreaming (void)
{
    /* Initialize Libraries */
    ContainerRecord::init();
    AncillaryFields::init();

    /* Add Lua Extensions */
    LuaEngine::extend(LUA_STREAMING_LIBNAME, stream_open, LIBID);

    /* Print Status */
    print2term("%s package initialized (%s)\n", LUA_STREAMING_LIBNAME, LIBID);
}

/*----------------------------------------------------------------------------
 * deinitstreaming
 *
 *  uninitialize streaming package
 *----------------------------------------------------------------------------*/
void deinitstreaming (void)
{
}
