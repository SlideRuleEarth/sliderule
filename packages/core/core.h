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

#ifndef __corepkg__
#define __corepkg__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

#include "Asset.h"
#include "AssetIndex.h"
#include "CaptureDispatch.h"
#include "ClusterSocket.h"
#include "ContainerRecord.h"
#include "CsvDispatch.h"
#include "DispatchObject.h"
#include "DeviceIO.h"
#include "DeviceObject.h"
#include "DeviceReader.h"
#include "DeviceWriter.h"
#include "Dictionary.h"
#include "EndpointObject.h"
#include "PointIndex.h"
#include "File.h"
#include "FileIODriver.h"
#include "HttpClient.h"
#include "HttpServer.h"
#include "LimitDispatch.h"
#include "List.h"
#include "LimitRecord.h"
#include "EventLib.h"
#include "LuaEndpoint.h"
#include "LuaEngine.h"
#include "LuaLibraryMsg.h"
#include "LuaLibrarySys.h"
#include "LuaLibraryTime.h"
#include "LuaObject.h"
#include "LuaScript.h"
#include "MathLib.h"
#include "MetricDispatch.h"
#include "MetricRecord.h"
#include "Monitor.h"
#include "MsgBridge.h"
#include "MsgProcessor.h"
#include "MsgQ.h"
#include "Ordering.h"
#include "PublisherDispatch.h"
#include "RecordObject.h"
#include "RecordDispatcher.h"
#include "ReportDispatch.h"
#include "SpatialIndex.h"
#include "StringLib.h"
#include "Table.h"
#include "TcpSocket.h"
#include "IntervalIndex.h"
#include "TimeLib.h"
#include "Uart.h"
#include "UdpSocket.h"

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

void    initcore        (void);
void    deinitcore      (void);
bool    checkactive     (void);
void    setinactive     (int errors = 0);
int     geterrors       (void);

#endif  /* __corepkg__ */
