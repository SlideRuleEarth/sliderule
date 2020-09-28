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

#ifndef __icesat2__
#define __icesat2__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "lua_parms.h"
#include "Atl03Reader.h"
#include "Atl03Indexer.h"
#include "Atl06Dispatch.h"
#include "GTArray.h"
#include "UT_Atl06Dispatch.h"

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

extern "C" {
void initicesat2 (void);
}

#endif  /* __icesat2__ */


