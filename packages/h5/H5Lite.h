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

#ifndef __h5_lite__
#define __h5_lite__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
#include "List.h"

/******************************************************************************
 * HDF5 I/O LITE LIBRARY
 ******************************************************************************/

struct H5Lite
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/
    
    static const int MAX_NDIMS = 2;
    static const long ALL_ROWS = -1;

    typedef enum {
        FILE,
        HSDS,
        S3,
        UNKNOWN
    } driver_t;

    typedef struct {
        int                     elements;   // number of elements in dataset
        int                     typesize;   // number of bytes per element
        int                     datasize;   // total number of bytes in dataset
        uint8_t*                data;       // point to allocated data buffer
        /* h5lite specific */
        RecordObject::valType_t datatype;   // high level data type
        int                     numcols;    // number of columns - anything past the second dimension is grouped togeher
        int                     numrows;    // number of rows - includes all dimensions after the first as a single row
    } info_t;

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void     init        (void);
    static void     deinit      (void);

    static driver_t parseUrl    (const char* url, const char** resource);
    static info_t   read        (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows);
    static bool     traverse    (const char* url, int max_depth, const char* start_group);


    protected:

        /******************************************************************************
         * HDF5 FILE BUFFER SUBCLASS
         ******************************************************************************/

        class H5FileBuffer
        {
            public:

                /*--------------------------------------------------------------------
                * Typedefs
                *--------------------------------------------------------------------*/
                
                typedef enum {
                    DATASPACE_MSG           = 0x1,
                    LINK_INFO_MSG           = 0x2,
                    DATATYPE_MSG            = 0x3,
                    FILL_VALUE_MSG          = 0x5,
                    LINK_MSG                = 0x6,
                    DATA_LAYOUT_MSG         = 0x8,
                    FILTER_MSG              = 0xB,
                    HEADER_CONT_MSG         = 0x10,
                    SYMBOL_TABLE_MSG        = 0x11
                } msg_type_t;

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
                    UNKNOWN_TYPE            = 11
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

                /*--------------------------------------------------------------------
                * Methods
                *--------------------------------------------------------------------*/

                                    H5FileBuffer        (info_t* data_info, const char* filename, const char* _dataset, long startrow, long numrows, bool _error_checking=false, bool _verbose=false);
                virtual             ~H5FileBuffer       (void);

            protected:

                /*--------------------------------------------------------------------
                * Constants
                *--------------------------------------------------------------------*/

                static const long       IO_BUFFSIZE            = 1048576; // 1MB
                static const long       STR_BUFF_SIZE           = 512;
                static const int        CHUNK_ALLOC_FACTOR      = 2;
                static const uint64_t   H5_SIGNATURE_LE         = 0x0A1A0A0D46444889LL;
                static const uint64_t   H5_OHDR_SIGNATURE_LE    = 0x5244484FLL; // object header
                static const uint64_t   H5_FRHP_SIGNATURE_LE    = 0x50485246LL; // fractal heap
                static const uint64_t   H5_FHDB_SIGNATURE_LE    = 0x42444846LL; // direct block
                static const uint64_t   H5_FHIB_SIGNATURE_LE    = 0x42494846LL; // indirect block
                static const uint64_t   H5_OCHK_SIGNATURE_LE    = 0x4B48434FLL; // object header continuation block
                static const uint64_t   H5_TREE_SIGNATURE_LE    = 0x45455254LL; // binary tree version 1
                static const uint64_t   H5_HEAP_SIGNATURE_LE    = 0x50414548LL; // local heap
                static const uint64_t   H5_SNOD_SIGNATURE_LE    = 0x444F4E53LL; // symbol table
                static const uint8_t    H5LITE_CUSTOM_V1_FLAG   = 0x80; // used to indicate version 1 object header (reserved)
                
                /*--------------------------------------------------------------------
                * Typedefs
                *--------------------------------------------------------------------*/

                typedef struct {
                    uint32_t                chunk_size;
                    uint32_t                filter_mask;
                    uint64_t                slice[MAX_NDIMS];
                    uint64_t                row_key;
                } btree_node_t;

                typedef struct {
                    int                     table_width;
                    int                     curr_num_rows;
                    int                     starting_blk_size;
                    int                     max_dblk_size;
                    int                     blk_offset_size;  // size in bytes of block offset field
                    bool                    dblk_checksum;
                    msg_type_t              msg_type;
                    int                     num_objects;
                    int                     cur_objects; // mutable
                } heap_info_t;

                typedef union {
                    double                  fill_lf;
                    float                   fill_f;
                    uint64_t                fill_ll;
                    uint32_t                fill_l;
                    uint16_t                fill_s;
                    uint8_t                 fill_b;
                } fill_t;

                /*--------------------------------------------------------------------
                * Methods
                *--------------------------------------------------------------------*/
                
                void                ioRequest           (uint8_t** data, int64_t size, uint64_t pos);

                void                parseDataset        (const char* _dataset);
                const char*         type2str            (data_type_t datatype);
                const char*         layout2str          (layout_t layout);
                int                 highestBit          (uint64_t value);
                int                 inflateChunk        (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size);
                int                 shuffleChunk        (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size, int type_size);
                uint64_t            readField           (int64_t size, uint64_t* pos);
                void                readData            (uint8_t* data, int64_t size, uint64_t* pos);
                void                readDataset         (info_t* _data_info);

                int                 readSuperblock      (void);        
                int                 readFractalHeap     (msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readDirectBlock     (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readIndirectBlock   (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readBTreeV1         (uint64_t pos, uint8_t* buffer, uint64_t buffer_size, uint64_t buffer_offset);
                btree_node_t        readBTreeNodeV1     (int ndims, uint64_t* pos);
                int                 readSymbolTable     (uint64_t pos, uint64_t heap_data_addr, int dlvl);

                int                 readObjHdr          (uint64_t pos, int dlvl);
                int                 readMessages        (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
                int                 readObjHdrV1        (uint64_t pos, int dlvl);
                int                 readMessagesV1      (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
                int                 readMessage         (msg_type_t type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl);

                int                 readDataspaceMsg    (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readLinkInfoMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readDatatypeMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readFillValueMsg    (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readLinkMsg         (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readDataLayoutMsg   (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readFilterMsg       (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readHeaderContMsg   (uint64_t pos, uint8_t hdr_flags, int dlvl);
                int                 readSymbolTableMsg  (uint64_t pos, uint8_t hdr_flags, int dlvl);
                
                /*--------------------------------------------------------------------
                * Data
                *--------------------------------------------------------------------*/

                const char*         dataset;
                List<const char*>   datasetPath;
                uint64_t            datasetStartRow;
                int                 datasetNumRows;
                bool                errorChecking;
                bool                verbose;

                /* File Management */
                fileptr_t           fp;
                uint8_t             ioBuffer[IO_BUFFSIZE];
                int64_t             ioBufferSize;
                uint64_t            ioCurrentPosition;

                /* File Meta Attributes */
                int                 offsetSize;
                int                 lengthSize;
                int                 groupLeafNodeK;
                int                 groupInternalNodeK;
                uint64_t            rootGroupOffset;

                /* Data Meta Attributes */
                data_type_t         dataType;
                int                 dataTypeSize;

                fill_t              dataFill;
                int                 dataFillSize;

                uint64_t            dataDimensions[MAX_NDIMS];
                int                 dataNumDimensions;

                bool                dataFilter[NUM_FILTERS]; // true if enabled for dataset
                uint32_t*           dataFilterParms[NUM_FILTERS];
                int                 dataNumFilterParms[NUM_FILTERS];

                layout_t            dataLayout;
                uint64_t            dataAddress;
                int64_t             dataSize;

                uint64_t            dataChunkElements; // number of data elements per chunk
                int                 dataChunkElementSize; // size of the data element in the chunk; should be equal to the typesize
                uint8_t*            dataChunkBuffer; // buffer for reading uncompressed chunk
                int64_t             dataChunkBufferSize; // dataChunkElements * dataInfo->typesize 
                uint8_t*            dataShuffleBuffer; // buffer for shuffling uncompressed chunk; same size as dataChunkBuffer

                uint8_t*            chunkBuffer; // buffer for reading raw and often compressed chunk data
                int64_t             chunkBufferSize; // variable size
        };
};

#endif  /* __h5_lite__ */
