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

/******************************************************************************
 * HDF5 DEFINES
 ******************************************************************************/

#ifndef H5CORO_MAXIMUM_NAME_SIZE
#define H5CORO_MAXIMUM_NAME_SIZE 208 // 104
#endif

/******************************************************************************
 * HDF5 FUTURE CLASS
 ******************************************************************************/

class H5Future
{
    public:

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t                    elements;   // number of elements in dataset
            uint32_t                    typesize;   // number of bytes per element
            uint64_t                    datasize;   // total number of bytes in dataset
            uint8_t*                    data;       // point to allocated data buffer
            RecordObject::fieldType_t   datatype;   // data type of elements
            int                         numcols;    // number of columns - anything past the second dimension is grouped together
            int                         numrows;    // number of rows - includes all dimensions after the first as a single row
        } info_t;

        typedef enum {
            INVALID     = -1,
            TIMEOUT     = 0,
            COMPLETE    = 1
        } rc_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                H5Future        (void);
                ~H5Future       (void);

        rc_t    wait            (int timeout); // ms
        void    finish          (bool _valid);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        info_t      info;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool        valid;      // set to false when error encountered
        bool        complete;   // set to true when data fully populated
        Cond        sync;       // signals when data read is complete
};

/******************************************************************************
 * HDF5 FILE BUFFER CLASS
 ******************************************************************************/

class H5FileBuffer
{
    public:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const long ALL_ROWS      = -1;
        static const int MAX_NDIMS      = 2;
        static const int FLAT_NDIMS     = 3;

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef H5Future::info_t info_t;

        /*--------------------------------------------------------------------
        * I/O Context (subclass)
        *--------------------------------------------------------------------*/

        typedef struct {
            uint8_t*                data;
            int64_t                 size;
            uint64_t                pos;
        } cache_entry_t;

        typedef Table<cache_entry_t, uint64_t> cache_t;

        struct io_context_t
        {
            cache_t     l1; // level 1 cache
            cache_t     l2; // level 2 cache
            Mutex       mut; // cache mutex
            long        pre_prefetch_request;
            long        post_prefetch_request;
            long        cache_miss;
            long        l1_cache_replace;
            long        l2_cache_replace;
            long        bytes_read;

            io_context_t    (void);
            ~io_context_t   (void);
        };

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

                            H5FileBuffer        (info_t* info, io_context_t* context, const Asset* asset, const char* resource, const char* dataset, long startrow, long numrows, bool _meta_only=false);
        virtual             ~H5FileBuffer       (void);

        // static void                decodeType5Record(const uint8_t *raw, void *_nrecord);
        // static void                decodeType8Record(const uint8_t *raw, void *_nrecord);
        // static void                compareType8Record(const void *_bt2_udata, const void *_bt2_rec, int *result);

    protected:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        /*
         * Assuming:
         *  50 regions of interest
         *  100 granuels per region
         *  30 datasets per granule
         *  200 bytes per dataset
         * Then:
         *  15000 datasets are needed
         *  30MB of data is used
         */

        static const long       MAX_META_STORE          = 150000;
        static const long       MAX_META_NAME_SIZE      = (H5CORO_MAXIMUM_NAME_SIZE & 0xFFF8); // forces size to multiple of 8

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

        static const long       STR_BUFF_SIZE           = 128;
        static const long       FILTER_SIZE_SCALE       = 1; // maximum factor for dataChunkFilterBuffer

        static const uint64_t   H5_SIGNATURE_LE                 = 0x0A1A0A0D46444889LL;
        static const uint64_t   H5_OHDR_SIGNATURE_LE            = 0x5244484FLL; // object header
        static const uint64_t   H5_FRHP_SIGNATURE_LE            = 0x50485246LL; // fractal heap
        static const uint64_t   H5_FHDB_SIGNATURE_LE            = 0x42444846LL; // direct block
        static const uint64_t   H5_FHIB_SIGNATURE_LE            = 0x42494846LL; // indirect block
        static const uint64_t   H5_OCHK_SIGNATURE_LE            = 0x4B48434FLL; // object header continuation block
        static const uint64_t   H5_TREE_SIGNATURE_LE            = 0x45455254LL; // binary tree version 1
        static const uint64_t   H5_HEAP_SIGNATURE_LE            = 0x50414548LL; // local heap
        static const uint64_t   H5_SNOD_SIGNATURE_LE            = 0x444F4E53LL; // symbol table
        static const uint64_t   H5_V2TREE_SIGNATURE_LE          = 0x44485442LL; // v2 btree header
        static const uint64_t   H5_V2TREE_INTERNAL_SIGNATURE_LE = 0x4E495442LL; // v2 internal node
        static const uint64_t   H5_V2TREE_LEAF_SIGNATURE_LE     = 0x464C5442LL; // v2 leaf node

        /* Object Header Flags */
        static const uint8_t    SIZE_OF_CHUNK_0_MASK    = 0x03;
        static const uint8_t    ATTR_CREATION_TRACK_BIT = 0x04;
        static const uint8_t    STORE_CHANGE_PHASE_BIT  = 0x10;
        static const uint8_t    FILE_STATS_BIT          = 0x20;
        static const uint8_t    H5CORO_CUSTOM_V1_FLAG   = 0x80; // used to indicate version 1 object header (reserved)

        static const int64_t    UNKNOWN_VALUE           = -1; // initial setting for variables prior to being set

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
            ATTRIBUTE_MSG           = 0xC,
            HEADER_CONT_MSG         = 0x10,
            SYMBOL_TABLE_MSG        = 0x11,
            ATTRIBUTE_INFO_MSG      = 0x15
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

        /* KAT MODIFIED */
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
            uint64_t                root_blk_addr; // kat mod
        } heap_info_t;

        /* KAT ADDED */

        /* Lookup table for general log2(n) routine */
        static constexpr const unsigned char LogTable256[] = {
            /* clang-clang-format off */
            0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
            /* clang-clang-format on */
        };

        typedef struct {
            uint32_t                chunk_size;
            uint32_t                filter_mask;
            uint64_t                slice[MAX_NDIMS];
            uint64_t                row_key;
        } btree_node_t;

        typedef struct {
            uint64_t fheap_addr;
            heap_info_t *fheap_info; // fractalH pointer
            const char *name; // attr name we are searching for
            uint32_t name_hash; // hash of attr name
            uint8_t flags; // attr storage location
            uint32_t corder; // creation order value of attribute to compare
            // H5A_bt2_found_t   found_op;      // callback when correct attribute is found
            // void             *found_op_data; // callback data when correct attribute is found
        } btree2_ud_common_t;

        /* A "node pointer" to another B-tree node */
        typedef struct {
            uint64_t addr; // address of pointed node
            uint16_t node_nrec; // num records in pointed node
            uint64_t all_nrec; // num records in pointed AND in children
        } btree2_node_ptr_t;

        /* Information about a node at a given depth */
        typedef struct {
            unsigned max_nrec; // max num records in node
            unsigned split_nrec; // num records to split node at 
            unsigned merge_nrec; // num records to merge node at
            uint64_t cum_max_nrec; // cumulative max. # of records below node's depth
            uint8_t cum_max_nrec_size; // size to store cumulative max. # of records for this node (in bytes)
            // H5FL_fac_head_t *nat_rec_fac; 
            // H5FL_fac_head_t *node_ptr_fac;
        } btree2_node_info_t;

        /* B-tree subID mapping for type support */
        typedef enum {
            H5B2_TEST_ID = 0,         /* B-tree is for testing (do not use for actual data) */
            H5B2_FHEAP_HUGE_INDIR_ID = 1, /* B-tree is for fractal heap indirectly accessed, non-filtered 'huge' objects*/
            H5B2_FHEAP_HUGE_FILT_INDIR_ID = 2, /* B-tree is for fractal heap indirectly accessed, filtered 'huge' objects */
            H5B2_FHEAP_HUGE_DIR_ID = 3, /* B-tree is for fractal heap directly accessed, non-filtered 'huge' objects */
            H5B2_FHEAP_HUGE_FILT_DIR_ID = 4, /* B-tree is for fractal heap directly accessed, filtered 'huge' objects */
            H5B2_GRP_DENSE_NAME_ID = 5,      /* B-tree is for indexing 'name' field for "dense" link storage in groups */
            H5B2_GRP_DENSE_CORDER_ID = 6,    /* B-tree is for indexing 'creation order' field for "dense" link storage ingroups */
            H5B2_SOHM_INDEX_ID = 7,          /* B-tree is an index for shared object header messages */
            H5B2_ATTR_DENSE_NAME_ID = 8,   /* B-tree is for indexing 'name' field for "dense" attribute storage on objects*/
            H5B2_ATTR_DENSE_CORDER_ID = 9, /* B-tree is for indexing 'creation order' field for "dense" attribute storage on objects */
            H5B2_CDSET_ID = 10,             /* B-tree is for non-filtered chunked dataset storage w/ >1 unlim dims */
            H5B2_CDSET_FILT_ID = 11,        /* B-tree is for filtered chunked dataset storage w/ >1 unlim dims */
            H5B2_TEST2_ID,             /* Another B-tree is for testing (do not use for actual data) */
            H5B2_NUM_BTREE_ID          /* Number of B-tree IDs (must be last)  */
        } btree2_subid_t;

        /* Node position, for min/max determination */
        typedef enum {
            H5B2_POS_ROOT,  /* Node is root (i.e. both right & left-most in tree) */
            H5B2_POS_RIGHT, /* Node is right-most in tree, at a given depth */
            H5B2_POS_LEFT,  /* Node is left-most in tree, at a given depth */
            H5B2_POS_MIDDLE /* Node is neither right or left-most in tree */
        } btree2_nodepos_t;

        /* B-tree header information */
        typedef struct {
            /* Tracking */
            uint64_t addr; // addr of btree 
            size_t hdr_size; // size (bytes) of btree on disk
            size_t node_refc; // ref count of nodes using header
            size_t file_refc; // ref count of files using header
            uint8_t sizeof_size; // size of file sizes
            uint8_t sizeof_addr; // size of file addresses
            uint8_t max_nrec_size; // size to store max. # of records in any node (in bytes)
            void *parent; // potentially remove

            /* Properties */

            // const btree2_type_t *cls;
            btree2_subid_t type; // "class" H5B2_class_t under hdf5 title
            size_t nrec_size; // native record size

            // callback methods - excluded store, context, encode, debug
            void (*compare)(const void *rec1, const void *rec2, int *result); // compare two native records
            void (*decode)(const uint8_t *raw, void *record); // decode record from disk

            uint32_t node_size; // size in bytes of all B-tree nodes
            uint32_t rrec_size; // size in bytes of the B-tree record
            uint16_t depth;
            uint8_t  split_percent; // percent full that a node needs to increase above before it is split
            uint8_t  merge_percent; // percent full that a node needs to be decrease below before it is split
            
            btree2_node_info_t *node_info;  // table of node info structs for current depth of B-tree
            btree2_node_ptr_t *root; // root struct
            size_t *nat_off; // array of offsets of native records
            uint64_t check_sum;

        } btree2_hdr_t;

        /* B-tree leaf node information */
        typedef struct {
            /* Internal B-tree Leaf information */
            btree2_hdr_t *hdr; // ptr to pinned header
            uint8_t      *leaf_native; // ptr to native records
            uint16_t     nrec; // num records in this node 
            void         *parent; // dependency for leaf  
        } btree2_leaf_t;

        /* B-tree internal node information */
        typedef struct {
            /* Internal B-tree information */
            btree2_hdr_t      *hdr; // ptr to pinned header
            uint8_t           *int_native; // ptr native records               
            btree2_node_ptr_t *node_ptrs; // ptr to node ptrs
            uint16_t          nrec; // num records in node
            uint16_t          depth; // depth of node
            void              *parent;
        } btree2_internal_t;

        /* Fractal heap ID type for shared message & attribute heap IDs. */
        typedef union {
            uint8_t  id[8]; /* Buffer to hold ID, for encoding/decoding */
            uint64_t val;  /* Value, for quick comparisons */
        } fheap_id_t;

        /* Type 8 Record Representation */
        typedef struct {
            fheap_id_t id; // heap ID for attribute
            uint8_t flags; // object header message flags for attribute
            uint32_t corder; // 'creation order' field value
            uint32_t hash; // hash of 'name' field value
        } btree2_type8_densename_rec_t;

        /* Type 5 Record Representation -  native 'name' field index records in the v2 B-tree */
        typedef struct {
            uint8_t  id[7]; // heap ID for link, 7 stolen from H5G_DENSE_FHEAP_ID_LEN
            uint32_t hash;  // hash of 'name' field value 
        } btree2_type5_densename_rec_t;

        /* Typedef for 'op' operations - REMOVED FOR CASE SWITCH */
        // typedef void (H5FileBuffer::*H5HF_operator_t)(const void *obj /*in*/, size_t obj_len, void *op_data /*in,out*/);

        typedef struct {
            const char                         *name; // name of attribute to compare
            const btree2_type8_densename_rec_t *record; // v2 B-tree record for attribute
            // H5A_bt2_found_t                 found_op;      /* Callback when correct attribute is found */
            // void                           *found_op_data; /* Callback data when correct attribute is found */
        } fheap_ud_cmp_t;

        /* END OF KAT ADDED */

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
            uint64_t                dimensions[MAX_NDIMS];
            uint64_t                chunkelements; // number of data elements per chunk
            uint64_t                chunkdims[MAX_NDIMS]; // dimension of each chunk
            uint64_t                address;
            int64_t                 size;
        } meta_entry_t;

        typedef Table<meta_entry_t, uint64_t> meta_repo_t;

       /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void                tearDown              (void);

        void                ioRequest             (uint64_t* pos, int64_t size, uint8_t* buffer, int64_t hint, bool cache);
        static bool         ioCheckCache          (uint64_t pos, int64_t size, cache_t* cache, uint64_t line_mask, cache_entry_t* entry);
        static uint64_t     ioHashL1              (uint64_t key);
        static uint64_t     ioHashL2              (uint64_t key);

        void                readByteArray         (uint8_t* data, int64_t size, uint64_t* pos);
        uint64_t            readField             (int64_t size, uint64_t* pos);
        void                readDataset           (info_t* info);

        uint64_t            readSuperblock        (void);
        int                 readFractalHeap       (msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl, heap_info_t* heap_info_ptr);
        int                 readDirectBlock       (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readIndirectBlock     (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readBTreeV1           (uint64_t pos, uint8_t* buffer, uint64_t buffer_size, uint64_t buffer_offset);
        btree_node_t        readBTreeNodeV1       (int ndims, uint64_t* pos);
        int                 readSymbolTable       (uint64_t pos, uint64_t heap_data_addr, int dlvl);

        int                 readObjHdr            (uint64_t pos, int dlvl);
        int                 readMessages          (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readObjHdrV1          (uint64_t pos, int dlvl);
        int                 readMessagesV1        (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readMessage           (msg_type_t type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl);

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
        static const char*  type2str              (data_type_t datatype);
        static const char*  layout2str            (layout_t layout);
        static int          highestBit            (uint64_t value);
        static int          inflateChunk          (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size);
        static int          shuffleChunk          (const uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_offset, uint32_t output_size, int type_size);

        static uint64_t     metaGetKey            (const char* url);
        static void         metaGetUrl            (char* url, const char* resource, const char* dataset);

        /* KAT ADDED METHODS */
        void                readDenseAttrs(uint64_t fheap_addr, uint64_t name_bt2_addr, const char *name, heap_info_t* heap_info_ptr);
        bool                isTypeSharedAttrs (unsigned type_id);
        void                uint32Decode(const uint8_t* p, uint32_t& i);
        void                addrDecode(size_t addr_len, const uint8_t **pp, uint64_t* addr_p);
        void                varDecode(uint8_t* p, int n, uint8_t l);
        unsigned            log2_gen(uint64_t n);

        void                decodeType5Record(const uint8_t *raw, void *_nrecord);
        void                decodeType8Record(const uint8_t *raw, void *_nrecord);
        void                compareType8Record(const void *_bt2_udata, const void *_bt2_rec, int *result);

        void                fheapLocate(heap_info_t *hdr, const void *_id, void *op_data);
        void                fheapLocate_Managed(heap_info_t* hdr, const uint8_t *id, void *op_data, unsigned op_flags);
        void                fheapNameCmp(const void *obj, size_t obj_len, void *op_data);
        void                locateRecordBTreeV2(btree2_hdr_t* hdr, unsigned nrec, size_t *rec_off, const uint8_t *native, const void *udata, unsigned *idx, int *cmp);
        void                openBTreeV2 (btree2_hdr_t *hdr, btree2_node_ptr_t *root_node_ptr, uint64_t addr);
        void                openInternalNode(btree2_internal_t *internal, btree2_hdr_t* hdr, uint64_t internal_pos, btree2_node_ptr_t* curr_node_ptr);
        void                findBTreeV2 (btree2_hdr_t* hdr, void* udata, bool *found);

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        /* Meta Repository */
        static meta_repo_t  metaRepo;
        static Mutex        metaMutex;

        /* Class Data */
        const char*         datasetName;            // holds buffer of dataset name that datasetPath points back into
        const char*         datasetPrint;           // holds untouched dataset name string used for displaying the name
        vector<const char*> datasetPath;
        uint64_t            datasetStartRow;
        int                 datasetNumRows;
        bool                errorChecking;
        bool                verbose;
        bool                metaOnly;

        /* I/O Management */
        Asset::IODriver*    ioDriver;
        char*               ioBucket;               // s3 driver
        char*               ioKey;                  // s3 driver
        io_context_t*       ioContext;
        bool                ioContextLocal;
        bool                ioPostPrefetch;

        /* File Info */
        uint8_t*            dataChunkBuffer;        // buffer for reading uncompressed chunk
        uint8_t*            dataChunkFilterBuffer;  // buffer for reading compressed chunk
        int64_t             dataChunkBufferSize;    // dataChunkElements * dataInfo->typesize
        int                 highestDataLevel;       // high water mark for traversing dataset path
        int64_t             dataSizeHint;

        /* Meta Info */
        meta_entry_t        metaData;
};

/******************************************************************************
 * HDF5 CLOUD-OPTIMIZED READ-ONLY LIBRARY
 ******************************************************************************/

struct H5Coro
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/

    static const long ALL_ROWS = H5FileBuffer::ALL_ROWS;
    static const long ALL_COLS = -1L;

    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/

    typedef H5Future::info_t info_t;
    typedef H5FileBuffer::io_context_t context_t;

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
