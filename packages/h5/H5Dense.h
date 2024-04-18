#include "H5Coro.h"

/******************************************************************************
 * HDF5 BTREEV2 CLASS
 ******************************************************************************/
class H5BTreeV2
{
    public:
        /* key return values for outside */

        uint64_t pos_out = 0; 
        uint8_t  hdr_flags_out = 0; 
        int      hdr_dlvl_out = 0; 
        uint64_t msg_size_out = 0;
        bool     found_attr = false;

        H5BTreeV2(uint64_t fheap_addr, uint64_t name_bt2_addr, const char *name, H5FileBuffer::heap_info_t* heap_info_ptr, H5FileBuffer* h5file);

        static unsigned log2_gen(uint64_t n);
        static uint16_t H5HF_SIZEOF_OFFSET_BITS(uint16_t b);

    protected:

        enum {
            H5O_MSG_FLAG_SHARED = 0x02u, // shared fractal heap flag
            H5O_FHEAP_ID_LEN = 8, // len for heap ID of attr
            H5HF_ID_VERS_MASK = 0xC0,
            H5HF_ID_VERS_CURR = 0x00,
            H5HF_ID_TYPE_MAN = 0x00, // manual object
            H5HF_ID_TYPE_HUGE = 0x10, // huge object
            H5HF_ID_TYPE_TINY = 0x20, // tiny object
            H5HF_ID_TYPE_RESERVED = 0x30,
            H5HF_ID_TYPE_MASK = 0x30,
            H5B2_METADATA_PREFIX_SIZE = 10, // used for h5btree v2 nrecord sizing
            H5B2_SIZEOF_RECORDS_PER_NODE = 2 // h5btree v2 records per node
        };

        typedef struct {
            uint64_t                fheap_addr = 0;
            H5FileBuffer::heap_info_t *fheap_info = nullptr; // TODO null ptr suggested // fractalH pointer
            const char              *name = nullptr; // attr name we are searching for
            uint32_t                name_hash = 0; // hash of attr name
            uint8_t                 flags = 0; // attr storage location
            uint32_t                corder = 0; // creation order value of attribute to compare
        } btree2_ud_common_t;

        /* A "node pointer" to another B-tree node */
        typedef struct {
            uint64_t                addr = 0; // address of pointed node
            uint16_t                node_nrec = 0; // num records in pointed node
            uint64_t                all_nrec = 0; // num records in pointed AND in children
        } btree2_node_ptr_t;

        /* Information about a node at a given depth */
        typedef struct {
            unsigned                max_nrec = 0; // max num records in node
            unsigned                split_nrec = 0; // num records to split node at 
            unsigned                merge_nrec = 0; // num records to merge node at
            uint64_t                cum_max_nrec = 0; // cumulative max. # of records below node's depth
            uint8_t                 cum_max_nrec_size = 0; // size to store cumulative max. # of records for this node (in bytes)
        } btree2_node_info_t;

        /* B-tree subID mapping for type support - represents record types */
        typedef enum {
            H5B2_TEST_ID = 0,        
            H5B2_FHEAP_HUGE_INDIR_ID = 1,
            H5B2_FHEAP_HUGE_FILT_INDIR_ID = 2, 
            H5B2_FHEAP_HUGE_DIR_ID = 3, 
            H5B2_FHEAP_HUGE_FILT_DIR_ID = 4,
            H5B2_GRP_DENSE_NAME_ID = 5,
            H5B2_GRP_DENSE_CORDER_ID = 6,
            H5B2_SOHM_INDEX_ID = 7,
            H5B2_ATTR_DENSE_NAME_ID = 8,
            H5B2_ATTR_DENSE_CORDER_ID = 9, 
            H5B2_CDSET_ID = 10,
            H5B2_CDSET_FILT_ID = 11,
            H5B2_TEST2_ID,
            H5B2_NUM_BTREE_ID
        } btree2_subid_t;

        /* Node position, for min/max determination */
        typedef enum {
            H5B2_POS_ROOT, // node is root (i.e. both right & left-most in tree)
            H5B2_POS_RIGHT, // node is right-most in tree, at a given depth
            H5B2_POS_LEFT, // node is left-most in tree, at a given depth
            H5B2_POS_MIDDLE // node is neither right or left-most in tree
        } btree2_nodepos_t;

        /* Doubling table for opening Direct/Indirect in Fractal heap */
        typedef struct {
            /* Derived information (stored, varies during lifetime of table) */
            uint64_t                table_addr = 0; // addr of first block for table u ndefined if no space allocated for table */
            unsigned                curr_root_rows = 0; // curr number of rows in the root indirect block; 0 indicates that the TABLE_ADDR field points to direct block (of START_BLOCK_SIZE) instead of indirect root block.

            /* Computed information (not stored) */
            unsigned                max_root_rows = 0; // max # of rows in root indirect block
            unsigned                max_direct_rows = 0; // max # of direct rows in any indirect block
            unsigned                start_bits = 0; // # of bits for starting block size (i.e. log2(start_block_size))
            unsigned                max_direct_bits = 0; // # of bits for max. direct block size (i.e. log2(max_direct_size))
            unsigned                max_dir_blk_off_size = 0; // max size of offsets in direct blocks
            unsigned                first_row_bits = 0; // # of bits in address of first row
            uint64_t                num_id_first_row = 0; // num of IDs in first row of table
            
            vector<uint64_t>        row_block_size; // block size per row of indirect block
            vector<uint64_t>        row_block_off; // cumulative offset per row of indirect block
            vector<uint64_t>        row_tot_dblock_free; // total free space in dblocks for this row  (For indirect block rows, it's the total free space in all direct blocks referenced from the indirect block)
            vector<uint64_t>        row_max_dblock_free; // max. free space in dblocks for this row (For indirect block rows, it's the maximum free space in a direct block referenced  from the indirect block)
        } dtable_t;

        /* B-tree leaf node information */
        typedef struct {
            // btree2_hdr_t            *hdr = nullptr;         // ptr to pinned header
            vector<uint8_t>         leaf_native;  // ptr to native records
            uint16_t                nrec = 0;         // num records in this node 
            void                    *parent = nullptr;      // dependency for leaf  
        } btree2_leaf_t;

        /* B-tree internal node information */
        typedef struct {
            // btree2_hdr_t              *hdr = nullptr; // ptr to pinned header
            vector<uint8_t>           int_native; // ptr native records               
            vector<btree2_node_ptr_t> node_ptrs;  // ptr to node ptrs
            uint16_t                  nrec = 0; // num records in node
            uint16_t                  depth = 0; // depth of node
            void                      *parent = nullptr;
        } btree2_internal_t;

        /* Fractal heap ID type for shared message & attribute heap IDs. */
        typedef union {
            uint8_t                 id[8]; // buf to hold ID, for encoding/decoding
            uint64_t                val; // for quick comparisons
        } fheap_id_t;

        /* Type 8 Record Representation */
        typedef struct {
            fheap_id_t              id; // heap ID for attribute
            uint8_t                 flags = 0;  // object header message flags for attribute
            uint32_t                corder = 0; // 'creation order' field value
            uint32_t                hash = 0; // hash of 'name' field value
        } btree2_type8_densename_rec_t;

        /* Type 5 Record Representation -  native 'name' field index records in the v2 B-tree */
        typedef struct {
            uint8_t                 id[7] = {0}; // heap ID for link, 7 stolen from H5G_DENSE_FHEAP_ID_LEN
            uint32_t                hash = 0; // hash of 'name' field value 
        } btree2_type5_densename_rec_t;

        /* Main Dense entry point */
        void                readDenseAttrs(uint64_t fheap_addr, const char *name, H5FileBuffer::heap_info_t* heap_info_ptr);

        /* Helpers */
        bool                isTypeSharedAttrs (unsigned type_id);
        uint32_t            checksumLookup3(const void *key, size_t length, uint32_t initval);
        template<typename T, typename V> bool checkAssigned(const T& type_verify, const V& value); 
        void                addrDecode(size_t addr_len, const uint8_t **pp, uint64_t* addr_p);
        void                varDecode(uint8_t* p, int n, uint8_t l);
        unsigned            log2_of2(uint32_t n);
        uint16_t            H5HF_SIZEOF_OFFSET_LEN(int l);
        uint32_t            H5_lookup3_rot(uint32_t x, uint32_t k);
        void                H5_lookup3_mix(uint32_t& a, uint32_t& b, uint32_t& c);
        void                H5_lookup3_final(uint32_t& a, uint32_t& b, uint32_t& c);

        /* Type Specific Decode/Comparators */
        void                decodeType5Record(const uint8_t *raw, void *_nrecord);
        uint64_t            decodeType8Record(uint64_t internal_pos, void *_nrecord);
        void                compareType8Record(const void *_bt2_udata, const void *_bt2_rec, int *result);

        /* Fheap Navigation*/
        void                fheapLocate(H5FileBuffer::heap_info_t *heap_info_ptr, const void * _id);
        void                fheapLocate_Managed(H5FileBuffer::heap_info_t* heap_info_ptr, uint8_t* id);
        void                fheapNameCmp(const void *obj, size_t obj_len, void *op_data);
        
        /* Btreev2 setting and navigation */
        void                locateRecordBTreeV2(unsigned nrec, size_t *rec_off, const uint8_t *native, const void *udata, unsigned *idx, int *cmp);
        void                initHdrBTreeV2(btree2_node_ptr_t *root_node_ptr);
        void                initDTable(H5FileBuffer::heap_info_t* heap_info_ptr, vector<uint64_t>& row_block_size, vector<uint64_t>& row_block_off, vector<uint64_t>& row_tot_dblock_free, vector<uint64_t>& row_max_dblock_free);
        void                initNodeInfo();
        void                openBTreeV2 (btree2_node_ptr_t *root_node_ptr, H5FileBuffer::heap_info_t* heap_info_ptr, void* udata);
        void                openInternalNode(btree2_internal_t *internal, uint64_t internal_pos, btree2_node_ptr_t* curr_node_ptr);
        void                findBTreeV2 (void* udata);
        uint64_t            openLeafNode(btree2_node_ptr_t *curr_node_ptr, btree2_leaf_t *leaf, uint64_t internal_pos);
        
        /* dtable search */
        void                dtableLookup(H5FileBuffer::heap_info_t* heap_info_ptr, uint64_t off, unsigned *row, unsigned *col);
        uint64_t            buildEntries_Indirect( H5FileBuffer::heap_info_t* heap_info, int nrows, uint64_t pos, uint64_t* ents);
        void                man_dblockLocate(H5FileBuffer::heap_info_t* heap_info_ptr, uint64_t obj_off, uint64_t* ents, unsigned *ret_entry);

    private:

        /* H5FileBuffer "This" pointer */
        /* NOTE: external instance H5FileBuffer is incomplete in construction */
        H5FileBuffer* h5filePtr_;

        /* B-tree header information */
        /* Tracking */
        uint64_t                addr = 0; // addr of btree 
        // size_t                  hdr_size = 0; // size (bytes) of btree on disk
        // size_t                  node_refc = 0; // ref count of nodes using header
        // size_t                  file_refc = 0; // ref count of files using header
        // uint8_t                 sizeof_size = 0; // size of file sizes
        // uint8_t                 sizeof_addr = 0; // size of file addresses
        uint8_t                 max_nrec_size = 0; // size to store max. # of records in any node (in bytes)
        void                    *parent = nullptr; // potentially remove

        /* Properties */
        btree2_subid_t          type; // "class" H5B2_class_t under hdf5 title
        size_t                  nrec_size = 0; // native record size

        uint32_t                node_size = 0; // size in bytes of all B-tree nodes
        uint16_t                rrec_size = 0; // size in bytes of the B-tree record
        uint16_t                depth = 0;
        uint8_t                 split_percent = 0; // percent full that a node needs to increase above before it is split
        uint8_t                 merge_percent = 0; // percent full that a node needs to be decrease below before it is split
        
        vector<btree2_node_info_t> node_info;  // table of node info structs for current depth of B-tree
        btree2_node_ptr_t       *root; // root struct
        vector<size_t>          nat_off;
        uint64_t                check_sum = 0;
        dtable_t                dtable; // doubling table


};