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

#ifndef __arrow_common__
#define __arrow_common__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "MsgQ.h"
#include "ArrowParms.h"


/******************************************************************************
 * NAMESPACES
 ******************************************************************************/
namespace ArrowCommon
{
    /******************************************************************************
     * TYPES
     ******************************************************************************/

    typedef struct WKBPoint {
        uint8_t                 byteOrder;
        uint32_t                wkbType;
        double                  x;
        double                  y;
    } ALIGN_PACKED wkbpoint_t;


    /******************************************************************************
     * METHODS
     ******************************************************************************/

    void        init       (void);
    bool        send2User  (const char* fileName, const char* outputPath,
                            uint32_t traceId, ArrowParms* parms, Publisher* outQ);
    bool        send2S3    (const char* fileName, const char* s3dst, const char* outputPath,
                            ArrowParms* parms, Publisher* outQ);
    bool        send2Client(const char* fileName, const char* outPath,
                            const ArrowParms* parms, Publisher* outQ);

    const char* getOutputPath(ArrowParms* parms);
    const char* getUniqueFileName(const char* id = NULL);
    char*       createMetadataFileName(const char* fileName);

    void        removeFile (const char* fileName);
    void        renameFile (const char* oldName, const char* newName);
    bool        fileExists (const char* fileName);

    int         luaSend2User (lua_State* L);
}

#endif  /* __arrow_common__ */
