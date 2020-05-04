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

#ifndef __legacy_pkg__
#define __legacy_pkg__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <string.h>

#include "CcsdsFileWriter.h"
#include "CcsdsFrameStripper.h"
#include "CcsdsMsgProcessor.h"
#include "CcsdsPacketProcessor.h"
#include "CcsdsProcessorModule.h"
#include "CcsdsPublisherProcessorModule.h"
#include "CcsdsRecordFileWriter.h"
#include "CfsInterface.h"
#include "CommandableObject.h"
#include "CommandProcessor.h"
#include "CosmosInterface.h"
#include "LuaInterpreter.h"
#include "LuaLibraryCmd.h"
#include "StatisticRecord.h"
#include "UT_Dictionary.h"
#include "UT_MsgQ.h"
#include "UT_TimeLib.h"

/******************************************************************************
 * dEFINES
 ******************************************************************************/

#define CMDQ "cmdq"

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

void initlegacy (void);
void deinitlegacy (void);

#endif  /* __legacy_pkg__ */
