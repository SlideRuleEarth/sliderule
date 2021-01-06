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

#ifndef __atlas__
#define __atlas__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

#include "atlasdefines.h"

#include "ItosRecord.h"
#include "ItosRecordParser.h"
#include "AtlasFileWriter.h"
#include "AtlasHistogram.h"
#include "AltimetryHistogram.h"
#include "TimeTagHistogram.h"
#include "TimeTagProcessorModule.h"
#include "AltimetryProcessorModule.h"
#include "MajorFrameProcessorModule.h"
#include "TimeProcessorModule.h"
#include "LaserProcessorModule.h"
#include "CmdEchoProcessorModule.h"
#include "DiagLogProcessorModule.h"
#include "HstvsSimulator.h"

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

extern "C" {
void initatlas (void);
}

#endif  /* __atlas__ */


