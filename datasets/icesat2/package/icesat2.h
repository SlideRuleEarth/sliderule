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

#ifndef __icesat2_plugin__
#define __icesat2_plugin__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Atl03Reader.h"
#include "Atl03Viewer.h"
#include "Atl03Indexer.h"
#include "Atl06Dispatch.h"
#include "Atl06Reader.h"
#include "Atl08Dispatch.h"
#include "Atl13IODriver.h"
#include "Atl13Reader.h"
#include "BathyClassifier.h"
#include "BathyFields.h"
#include "BathyOceanEyes.h"
#include "BathyReader.h"
#include "BathyViewer.h"
#include "CumulusIODriver.h"
#include "GTArray.h"
#include "GTDArray.h"
#include "Icesat2Parms.h"
#include "MeritRaster.h"

#ifdef __unittesting__
#include "UT_Atl03Reader.h"
#include "UT_Atl06Dispatch.h"
#endif

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

extern "C" {
void initicesat2 (void);
void deiniticesat2 (void);
}

#endif  /* __icesat2_plugin__ */


