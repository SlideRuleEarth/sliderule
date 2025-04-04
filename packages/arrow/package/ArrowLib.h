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

#ifndef __arrow_lib__
#define __arrow_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "MsgQ.h"
#include "ArrowFields.h"
#include "RecordObject.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

struct ArrowLib
{
    /******************************************************************************
     * Constants
     ******************************************************************************/

     static const int URL_MAX_LEN = 512;
     static const int FILE_NAME_MAX_LEN = 128;
     static const int FILE_BUFFER_RSPS_SIZE = 0x2000000; // 32MB

     static const char* metaRecType;
     static const char* dataRecType;
     static const char* eofRecType;
     static const char* remoteRecType;

     static const RecordObject::fieldDef_t metaRecDef[];
     static const RecordObject::fieldDef_t dataRecDef[];
     static const RecordObject::fieldDef_t eofRecDef[];
     static const RecordObject::fieldDef_t remoteRecDef[];

    /******************************************************************************
     * Typedefs
     ******************************************************************************/

    typedef struct {
        char        filename[FILE_NAME_MAX_LEN];
        long        size;
    } arrow_file_meta_t;

    typedef struct {
        char        filename[FILE_NAME_MAX_LEN];
        uint8_t     data[];
    } arrow_file_data_t;

    typedef struct {
        char        filename[FILE_NAME_MAX_LEN];
        uint64_t    checksum;
    } arrow_file_eof_t;

    typedef struct {
        char        url[URL_MAX_LEN];
        long        size;
    } arrow_file_remote_t;

    typedef struct WKBPoint {
        uint8_t                 byteOrder;
        uint32_t                wkbType;
        double                  x;
        double                  y;
    } ALIGN_PACKED wkbpoint_t;

    /******************************************************************************
     * METHODS
     ******************************************************************************/

    static void        init         (void);
    static bool        send2User    (const char* fileName, const char* outputPath, uint32_t traceId, const ArrowFields* parms, Publisher* outQ);
    static bool        send2S3      (const char* fileName, const char* s3dst, const char* outputPath, const ArrowFields* parms, Publisher* outQ);
    static bool        send2Client  (const char* fileName, const char* outPath, const ArrowFields* parms, Publisher* outQ);

    static const char* getUniqueFileName        (const char* id = NULL);
    static char*       createMetadataFileName   (const char* fileName);

    static void        removeFile   (const char* fileName);
    static void        renameFile   (const char* oldName, const char* newName);
    static bool        fileExists   (const char* fileName);

    static int         luaSend2User (lua_State* L);
};

#endif  /* __arrow_lib__ */
