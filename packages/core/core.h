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

#ifndef __corepkg__
#define __corepkg__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

#include "AssetIndex.h"
#include "CaptureDispatch.h"
#include "ClusterSocket.h"
#include "DispatchObject.h"
#include "DeviceIO.h"
#include "DeviceObject.h"
#include "DeviceReader.h"
#include "DeviceWriter.h"
#include "Dictionary.h"
#include "EndpointObject.h"
#include "File.h"
#include "HttpServer.h"
#include "LimitDispatch.h"
#include "List.h"
#include "LimitRecord.h"
#include "Logger.h"
#include "LogLib.h"
#include "LuaEndpoint.h"
#include "LuaEngine.h"
#include "LuaLibraryMsg.h"
#include "LuaLibrarySys.h"
#include "LuaLibraryTime.h"
#include "LuaObject.h"
#include "MetricDispatch.h"
#include "MetricRecord.h"
#include "MsgProcessor.h"
#include "MsgQ.h"
#include "Ordering.h"
#include "PublisherDispatch.h"
#include "RecordObject.h"
#include "RecordDispatcher.h"
#include "ReportDispatch.h"
#include "StringLib.h"
#include "TcpSocket.h"
#include "TimeLib.h"
#include "TraceLib.h"
#include "Uart.h"
#include "UdpSocket.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef CONFIGPATH
#define CONFIGPATH "."
#endif

#ifndef EXECPATH
#define EXECPATH "."
#endif

#ifndef LIBPATH
#define LIBPATH "."
#endif

#ifndef INCPATH
#define INCPATH "."
#endif

#ifndef BINID
#define BINID "local"
#endif

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

void    initcore    (void);
void    deinitcore  (void);
bool    checkactive (void);
void    setinactive (void);

#endif  /* __corepkg__ */
