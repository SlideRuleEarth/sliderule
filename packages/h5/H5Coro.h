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
#include "Asset.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "Table.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef H5CORO_MAXIMUM_DIMENSIONS
#define H5CORO_MAXIMUM_DIMENSIONS 3
#endif

#ifndef H5CORO_MAXIMUM_NAME_SIZE
#define H5CORO_MAXIMUM_NAME_SIZE 104
#endif

#ifndef H5CORO_VERBOSE
#define H5CORO_VERBOSE true
#endif

#ifndef H5CORO_EXTRA_DEBUG
#define H5CORO_EXTRA_DEBUG true
#endif

#ifndef H5CORO_ERROR_CHECKING
#define H5CORO_ERROR_CHECKING true
#endif

#ifndef H5CORO_ENABLE_FILL
#define H5CORO_ENABLE_FILL false
#endif

/******************************************************************************
 * MACRO
 ******************************************************************************/

#define COLUMN_SLICE(col,startrow,numrows) {{startrow, startrow + numrows}, {col == H5Coro::ALL_COLS ? 0 : col, col == H5Coro::ALL_COLS ? H5Coro::EOR : col + 1}}

/******************************************************************************
 * H5CORO CLASS
 ******************************************************************************/

namespace H5Coro
{
    /*--------------------------------------------------------------------
    * Constants
    *--------------------------------------------------------------------*/

    const int MAX_NDIMS = H5CORO_MAXIMUM_DIMENSIONS;    
    const long EOR = -1L; // end of range - read rest of the span of a dimension
    const long ALL_ROWS = EOR;
    const long ALL_COLS = EOR;

    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/

    typedef struct {
        uint32_t                    elements;   // number of elements in dataset
        uint32_t                    typesize;   // number of bytes per element
        uint64_t                    datasize;   // total number of bytes in dataset
        uint8_t*                    data;       // point to allocated data buffer
        RecordObject::fieldType_t   datatype;   // data type of elements
        int64_t                     shape[MAX_NDIMS]; // dimensions of the data
    } info_t;

    typedef struct {                    // [r0, r1)
        int64_t                     r0; // start of slice
        int64_t                     r1; // end of slice
    } range_t;

    /*--------------------------------------------------------------------
    * Future Subclass
    *--------------------------------------------------------------------*/

    class Future
    {
        public:

            typedef enum {
                INVALID     = -1,
                TIMEOUT     = 0,
                COMPLETE    = 1
            } rc_t;

                    Future          (void);
                    ~Future         (void);
            rc_t    wait            (int timeout); // ms
            void    finish          (bool _valid);

            info_t  info;

        private:

            bool    valid;      // set to false when error encountered
            bool    complete;   // set to true when data fully populated
            Cond    sync;       // signals when data read is complete
    };

    /*--------------------------------------------------------------------
    * Context Subclass
    *--------------------------------------------------------------------*/

    struct Context
    {
        /*************/
        /* Constants */
        /*************/

        /*
         * Assuming:
         *  50ms of latency per read
         * Then per throughput:
         *  ~500Mbits/second --> 1MB (L1 LINESIZE)
         *  ~2Gbits/second --> 8MB (L1 LINESIZE)
         */
        static const int64_t    IO_CACHE_L1_LINESIZE    = 0x100000; // 1MB cache line
        static const uint64_t   IO_CACHE_L1_MASK        = 0x0FFFFF; // lower inverse of buffer size
        static const long       IO_CACHE_L1_ENTRIES     = 157; // cache lines per dataset
        static const uint64_t   IO_CACHE_L2_MASK        = 0x7FFFFFF; // lower inverse of buffer size
        static const long       IO_CACHE_L2_ENTRIES     = 17; // cache lines per dataset

        /************/
        /* Typedefs */
        /************/

        typedef struct {
            uint8_t*    data;
            int64_t     size;
            uint64_t    pos;
        } cache_entry_t;

        typedef Table<cache_entry_t, uint64_t> cache_t;

        /********/
        /* Data */
        /********/

        const char*         name;
        Asset::IODriver*    ioDriver;
        cache_t             l1;                     // level 1 cache
        cache_t             l2;                     // level 2 cache
        Mutex               mut;                    // cache mutex
        long                cache_miss;
        long                l1_cache_replace;
        long                l2_cache_replace;
        long                bytes_read;

        /***********/
        /* Methods */
        /***********/

                        Context     (Asset* asset, const char* resource);
                        ~Context    (void);

        void            ioRequest   (uint64_t* pos, int64_t size, uint8_t* buffer, int64_t hint, bool cache);
        static bool     checkCache  (uint64_t pos, int64_t size, cache_t* cache, uint64_t line_mask, cache_entry_t* entry);
        static uint64_t hashL1      (uint64_t key);
        static uint64_t hashL2      (uint64_t key);
    };

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    void        init            (int num_threads);
    void        deinit          (void);

    info_t      read            (Context* context, const char* datasetname, RecordObject::valType_t valtype, const range_t* slice, int slicendims, bool _meta_only=false, uint32_t parent_trace_id=ORIGIN);
    Future*     readp           (Context* context, const char* datasetname, RecordObject::valType_t valtype, const range_t* slice, int slicendims);
    void*       readerThread    (void* parm);
};

#endif  /* __h5coro__ */
