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

#ifndef __h5coro__
#define __h5coro__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
#include "List.h"
#include "Table.h"
#include "Asset.h"
#include "MsgQ.h"
#include "H5Dataset.h"
#include "H5Future.h"

/******************************************************************************
 * H5CORO CLASS
 ******************************************************************************/

struct H5Coro
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/

    static const long ALL_ROWS = H5Dataset::ALL_ROWS;
    static const long ALL_COLS = -1L;

    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/

    typedef H5Future::info_t info_t;
    typedef H5Dataset::io_context_t context_t;

    typedef struct {
        const Asset*            asset;
        const char*             resource;
        const char*             datasetname;
        RecordObject::valType_t valtype;
        long                    col;
        long                    startrow;
        long                    numrows;
        context_t*              context;
        uint32_t                traceid;
        H5Future*               h5f;
    } read_rqst_t;

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void         init            (int num_threads);
    static void         deinit          (void);
    static info_t       read            (const Asset* asset, const char* resource, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context=NULL, bool _meta_only=false, uint32_t parent_trace_id=ORIGIN);
    static bool         traverse        (const Asset* asset, const char* resource, int max_depth, const char* start_group);

    static H5Future*    readp           (const Asset* asset, const char* resource, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context=NULL);
    static void*        reader_thread   (void* parm);

    /*--------------------------------------------------------------------
     * Data
     *--------------------------------------------------------------------*/

    static Publisher*   rqstPub;
    static Subscriber*  rqstSub;
    static bool         readerActive;
    static Thread**     readerPids; // thread pool
    static int          threadPoolSize;
};

#endif  /* __h5coro__ */
