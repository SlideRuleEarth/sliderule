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

#ifndef __h5dataset__
#define __h5dataset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "H5Coro.h"

using H5Coro::info_t;
using H5Coro::range_t;
using H5Coro::Context;
using H5Coro::MAX_NDIMS;

/******************************************************************************
 * H5DATASET CLASS
 ******************************************************************************/

class H5Dataset
{
    public:

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef enum {
            DATASPACE_MSG       = 0x1,
            LINK_INFO_MSG       = 0x2,
            DATATYPE_MSG        = 0x3,
            FILL_VALUE_MSG      = 0x5,
            LINK_MSG            = 0x6,
            DATA_LAYOUT_MSG     = 0x8,
            FILTER_MSG          = 0xB,
            ATTRIBUTE_MSG       = 0xC,
            HEADER_CONT_MSG     = 0x10,
            SYMBOL_TABLE_MSG    = 0x11,
            ATTRIBUTE_INFO_MSG  = 0x15
        } msg_type_t;

        typedef struct {
            int             table_width;
            int             curr_num_rows;
            int             starting_blk_size;
            int             max_dblk_size;
            int             blk_offset_size;  // size in bytes of block offset field
            bool            dblk_checksum;
            msg_type_t      msg_type;
            int             num_objects;
            int             cur_objects; // mutable
            uint64_t        root_blk_addr;
            uint32_t        max_size_mg_obj;
            uint16_t        max_heap_size;
            uint8_t         hdr_flags;
            uint8_t         heap_off_size; // uint32_t Size of heap offsets (in bytes)
            uint8_t         heap_len_size; // size of heap ID lengths (in bytes)
            int             dlvl; // pass down to found message for dense
        } heap_info_t;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

                H5Dataset   (info_t* info, Context* context,
                             const char* dataset, const range_t* slice, int slicendims,
                             bool _meta_only=false);
        virtual ~H5Dataset  (void);

    protected:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const long       MAX_META_STORE                  = 150000;
        static const long       MAX_META_NAME_SIZE              = (H5CORO_MAXIMUM_NAME_SIZE & 0xFFF8); // forces size to multiple of 8

        static const long       STR_BUFF_SIZE                   = 128;
        static const long       FILTER_SIZE_SCALE               = 1; // maximum factor for dataChunkFilterBuffer

        static const uint64_t   H5_SIGNATURE_LE                 = 0x0A1A0A0D46444889LL;
        static const uint64_t   H5_OHDR_SIGNATURE_LE            = 0x5244484FLL; // object header
        static const uint64_t   H5_FRHP_SIGNATURE_LE            = 0x50485246LL; // fractal heap
        static const uint64_t   H5_FHDB_SIGNATURE_LE            = 0x42444846LL; // direct block
        static const uint64_t   H5_FHIB_SIGNATURE_LE            = 0x42494846LL; // indirect block
        static const uint64_t   H5_OCHK_SIGNATURE_LE            = 0x4B48434FLL; // object header continuation block
        static const uint64_t   H5_TREE_SIGNATURE_LE            = 0x45455254LL; // binary tree version 1
        static const uint64_t   H5_HEAP_SIGNATURE_LE            = 0x50414548LL; // local heap
        static const uint64_t   H5_SNOD_SIGNATURE_LE            = 0x444F4E53LL; // symbol table
        static const uint64_t   H5_GCOL_SIGNATURE_LE            = 0x4C4F4347LL; // global collection
        static const uint64_t   H5_V2TREE_SIGNATURE_LE          = 0x44485442LL; // v2 btree header
        static const uint64_t   H5_V2TREE_INTERNAL_SIGNATURE_LE = 0x4E495442LL; // v2 internal node
        static const uint64_t   H5_V2TREE_LEAF_SIGNATURE_LE     = 0x464C5442LL; // v2 leaf node

        /* Object Header Flags */
        static const uint8_t    SIZE_OF_CHUNK_0_MASK            = 0x03;
        static const uint8_t    ATTR_CREATION_TRACK_BIT         = 0x04;
        static const uint8_t    STORE_CHANGE_PHASE_BIT          = 0x10;
        static const uint8_t    FILE_STATS_BIT                  = 0x20;
        static const uint8_t    H5CORO_CUSTOM_V1_FLAG           = 0x80; // used to indicate version 1 object header (reserved)

        static const int64_t    UNKNOWN_VALUE                   = -1; // initial setting for variables prior to being set

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef enum {
            FIXED_POINT_TYPE        = 0,
            FLOATING_POINT_TYPE     = 1,
            TIME_TYPE               = 2,
            STRING_TYPE             = 3,
            BIT_FIELD_TYPE          = 4,
            OPAQUE_TYPE             = 5,
            COMPOUND_TYPE           = 6,
            REFERENCE_TYPE          = 7,
            ENUMERATED_TYPE         = 8,
            VARIABLE_LENGTH_TYPE    = 9,
            ARRAY_TYPE              = 10,
            UNKNOWN_TYPE            = 11,
            VL_STRING_TYPE          = 12,
            VL_SEQUENCE_TYPE        = 13
        } data_type_t;

        typedef enum {
            COMPACT_LAYOUT          = 0,
            CONTIGUOUS_LAYOUT       = 1,
            CHUNKED_LAYOUT          = 2,
            UNKNOWN_LAYOUT          = 3
        } layout_t;

        typedef enum {
            INVALID_FILTER          = 0,
            DEFLATE_FILTER          = 1,
            SHUFFLE_FILTER          = 2,
            FLETCHER32_FILTER       = 3,
            SZIP_FILTER             = 4,
            NBIT_FILTER             = 5,
            SCALEOFFSET_FILTER      = 6,
            NUM_FILTERS             = 7
        } filter_t;

        typedef struct {
            uint32_t                chunk_size;
            uint32_t                filter_mask;
            int64_t                 slice[MAX_NDIMS];
        } btree_node_t;

        typedef union {
            double                  fill_lf;
            float                   fill_f;
            uint64_t                fill_ll;
            uint32_t                fill_l;
            uint16_t                fill_s;
            uint8_t                 fill_b;
        } fill_t;

        typedef struct {
            char                    url[MAX_META_NAME_SIZE];
            data_type_t             type;
            layout_t                layout;
            fill_t                  fill;
            bool                    filter[NUM_FILTERS]; // true if enabled for dataset
            bool                    signedval; // is the value a signed or not
            int                     typesize;
            int                     fillsize;
            int                     ndims;
            int                     elementsize; // size of the data element in the chunk; should be equal to the typesize
            int                     offsetsize; // size of "offset" fields in h5 files
            int                     lengthsize; // size of "length" fields in h5 files
            int64_t                 dimensions[MAX_NDIMS];
            int64_t                 chunkelements; // number of data elements per chunk
            int64_t                 chunkdims[MAX_NDIMS]; // dimension of each chunk
            uint64_t                address;
            int64_t                 size;
        } meta_entry_t;

        typedef Table<meta_entry_t, uint64_t> meta_repo_t;

       /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void                tearDown              (void);

        void                readByteArray         (uint8_t* data, int64_t size, uint64_t* pos);
        uint64_t            readField             (int64_t size, uint64_t* pos);
        void                readDataset           (info_t* info);

        uint64_t            readSuperblock        (void);
        int                 readFractalHeap       (msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl, heap_info_t* heap_info_ptr);
        int                 readDirectBlock       (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readIndirectBlock     (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readBTreeV1           (uint64_t pos, uint8_t* buffer, uint64_t buffer_size);
        btree_node_t        readBTreeNodeV1       (int ndims, uint64_t* pos);
        int                 readSymbolTable       (uint64_t pos, uint64_t heap_data_addr, int dlvl);

        int                 readObjHdr            (uint64_t pos, int dlvl);
        int                 readMessages          (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readObjHdrV1          (uint64_t pos, int dlvl);
        int                 readMessagesV1        (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readMessage           (msg_type_t type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        uint64_t            readVLString          (uint64_t pos, uint8_t** buffer);

        int                 readDataspaceMsg      (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readLinkInfoMsg       (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readDatatypeMsg       (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readFillValueMsg      (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readLinkMsg           (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readDataLayoutMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readFilterMsg         (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readAttributeMsg      (uint64_t pos, uint8_t hdr_flags, int dlvl, uint64_t size);
        int                 readHeaderContMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readSymbolTableMsg    (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readAttributeInfoMsg  (uint64_t pos, uint8_t hdr_flags, int dlvl);

        void                parseDataset          (void);
        bool                hypersliceIntersection(const range_t* node_slice, const uint8_t node_level);
        void                readSlice             (uint8_t* output_buffer, const int64_t* output_dimensions, const range_t* output_slice,
                                                   const uint8_t* input_buffer, const int64_t* input_dimensions, const range_t* input_slice) const;

        static const char*  type2str              (data_type_t datatype);
        static const char*  layout2str            (layout_t layout);
        static int          highestBit            (uint64_t value);
        static int          inflateChunk          (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size);
        static int          shuffleChunk          (const uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_offset, uint32_t output_size, int type_size);

        static uint64_t     metaGetKey            (const char* url);
        static void         metaGetUrl            (char* url, const char* resource, const char* dataset);

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        /* Meta Repository */
        static meta_repo_t  metaRepo;
        static Mutex        metaMutex;

        /* Class Data */
        Context*            ioContext;
        const char*         datasetName;            // holds buffer of dataset name that datasetPath points back into
        const char*         datasetPrint;           // holds untouched dataset name string used for displaying the name
        vector<const char*> datasetPath;
        range_t             hyperslice[MAX_NDIMS];
        int64_t             shape[MAX_NDIMS];
        bool                metaOnly;

        /* File Info */
        uint8_t*            dataChunkBuffer;            // buffer for reading uncompressed chunk
        uint8_t*            dataChunkFilterBuffer;      // buffer for reading compressed chunk
        int64_t             dataChunkBufferSize;        // dataChunkElements * dataInfo->typesize
        int64_t             dataChunkFilterBufferSize;  // dataChunkBufferSize * FILTER_SIZE_SCALE
        int                 highestDataLevel;           // high water mark for traversing dataset path
        int64_t             dataSizeHint;
        int64_t             dimensionsInChunks[MAX_NDIMS];
        int64_t             chunkStepSize[MAX_NDIMS];
        int64_t             hypersliceChunkStart;
        int64_t             hypersliceChunkEnd;

        /* Meta Info */
        meta_entry_t        metaData;

    friend class H5BTreeV2;
};

#endif