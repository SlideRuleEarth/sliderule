#include "H5Coro.h"
#include "H5Dataset.h"

/******************************************************************************
 * HDF5 BTREEV2 CLASS
 ******************************************************************************/
class H5BTreeV2
{
    public:
        /* key return values for outside */

        uint64_t pos_out;
        uint64_t msg_size_out;
        int32_t  hdr_dlvl_out;
        uint8_t  hdr_flags_out;
        bool     found_attr;

        H5BTreeV2(uint64_t _fheap_addr, uint64_t name_bt2_addr, const char *_name, H5Dataset::heap_info_t* heap_info_ptr, H5Dataset* h5file);

        static uint32_t log2Gen(uint64_t n);
        static uint16_t sizeOffsetBits(uint16_t b);

    protected:

        enum {
            H5O_MSG_FLAG_SHARED = 0x02U, // shared fractal heap flag
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

        /* A "node pointer" to another B-tree node */
        typedef struct btree2_node_ptr_t{
            uint64_t                addr = 0; // address of pointed node
            uint64_t                all_nrec = 0; // num records in pointed AND in children
            uint16_t                node_nrec = 0; // num records in pointed node
        } btree2_node_ptr_t;

        /* Information about a node at a given depth */
        typedef struct btree2_node_info_t {
            uint64_t                cum_max_nrec = 0; // cumulative max. # of records below node's depth
            uint32_t                max_nrec = 0; // max num records in node
            uint32_t                split_nrec = 0; // num records to split node at
            uint32_t                merge_nrec = 0; // num records to merge node at
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
        typedef struct dtable_t {
            /* Derived information (stored, varies during lifetime of table) */
            uint64_t                table_addr = 0; // addr of first block for table u ndefined if no space allocated for table */
            uint64_t                num_id_first_row = 0; // num of IDs in first row of table

            vector<uint64_t>        row_block_size; // block size per row of indirect block
            vector<uint64_t>        row_block_off; // cumulative offset per row of indirect block
            vector<uint64_t>        row_tot_dblock_free; // total free space in dblocks for this row  (For indirect block rows, it's the total free space in all direct blocks referenced from the indirect block)
            vector<uint64_t>        row_max_dblock_free; // max. free space in dblocks for this row (For indirect block rows, it's the maximum free space in a direct block referenced  from the indirect block)

            uint32_t                curr_root_rows = 0; // curr number of rows in the root indirect block; 0 indicates that the TABLE_ADDR field points to direct block (of START_BLOCK_SIZE) instead of indirect root block.
            uint32_t                max_root_rows = 0; // max # of rows in root indirect block
            uint32_t                max_direct_rows = 0; // max # of direct rows in any indirect block
            uint32_t                start_bits = 0; // # of bits for starting block size (i.e. log2(start_block_size))
            uint32_t                max_direct_bits = 0; // # of bits for max. direct block size (i.e. log2(max_direct_size))
            uint32_t                max_dir_blk_off_size = 0; // max size of offsets in direct blocks
            uint32_t                first_row_bits = 0; // # of bits in address of first row

        } dtable_t;

        /* B-tree leaf node information */
        typedef struct btree2_leaf_t {
            vector<uint8_t>         leaf_native;    // ptr to native records
            void                    *parent = NULL; // dependency for leaf
            uint16_t                nrec = 0;       // num records in this node
        } btree2_leaf_t;

        /* B-tree internal node information */
        typedef struct btree2_internal_t {
            void                      *parent = NULL;
            vector<uint8_t>           int_native; // ptr native records
            vector<btree2_node_ptr_t> node_ptrs;  // ptr to node ptrs
            uint16_t                  nrec = 0; // num records in node
            uint16_t                  depth = 0; // depth of node
        } btree2_internal_t;

        /* Fractal heap ID type for shared message & attribute heap IDs. */
        typedef union {
            uint8_t                 id[8]; // buf to hold ID, for encoding/decoding
            uint64_t                val; // for quick comparisons
        } fheap_id_t;

        /* Type 8 Record Representation */
        typedef struct btree2_type8_densename_rec_t {
            fheap_id_t              id; // heap ID for attribute
            uint32_t                corder = 0; // 'creation order' field value
            uint32_t                hash = 0; // hash of 'name' field value
            uint8_t                 flags = 0;  // object header message flags for attribute
        } btree2_type8_densename_rec_t;

        /* Type 5 Record Representation -  native 'name' field index records in the v2 B-tree */
        typedef struct btree2_type5_densename_rec_t {
            uint32_t                hash = 0; // hash of 'name' field value
            uint8_t                 id[7] = {0}; // heap ID for link, 7 stolen from H5G_DENSE_FHEAP_ID_LEN
        } btree2_type5_densename_rec_t;

        /* Helpers */
        static bool         isTypeSharedAttrs (uint32_t type_id);
        static uint32_t     checksumLookup3(const void *key, size_t length, uint32_t initval);
        template<typename T, typename V> void safeAssigned(T& type_verify, V& value);
        static void         addrDecode(size_t addr_len, const uint8_t **pp, uint64_t* addr_p);
        static void         varDecode(uint8_t* p, int32_t n, uint8_t l);
        static uint32_t     log2Of2(uint32_t n);
        static uint16_t     sizeOffsetLen(int32_t l);
        static uint32_t     lookup3Rot(uint32_t x, uint32_t k);
        static void         lookup3Mix(uint32_t& a, uint32_t& b, uint32_t& c);
        static void                lookup3Final(uint32_t& a, uint32_t& b, uint32_t& c);

        /* Type Specific Decode/Comparators */
        static void         decodeType5Record(const uint8_t *raw, void *_nrecord);
        uint64_t            decodeType8Record(uint64_t internal_pos, void *_nrecord);
        void                compareType8Record(const void *_bt2_rec, int32_t *result);

        /* Fheap Navigation*/
        void                fheapLocate(const void * _id);
        void                fheapLocateManaged(uint8_t* id);
        static void         fheapNameCmp(const void *obj, size_t obj_len, const void *op_data);

        /* Btreev2 setting and navigation */
        void                locateRecordBTreeV2(uint32_t nrec, const size_t *rec_off, const uint8_t *native, uint32_t *idx, int32_t *cmp);
        void                openInternalNode(btree2_internal_t *internal, uint64_t internal_pos, const btree2_node_ptr_t* curr_node_ptr);
        void                findBTreeV2();
        uint64_t            openLeafNode(const btree2_node_ptr_t *curr_node_ptr, btree2_leaf_t *leaf, uint64_t internal_pos);

        /* dtable search */
        void                dtableLookup(uint64_t off, uint32_t *row, uint32_t *col);
        uint64_t            buildEntriesIndirect(int32_t nrows, uint64_t pos, uint64_t* ents);
        void                manualDblockLocate(uint64_t obj_off, uint64_t* ents, uint32_t *ret_entry);

    private:

        /* H5FILEBUFFER POINTER*/
        /* NOTE: external instance H5Dataset is incomplete in construction */
        H5Dataset* h5filePtr_;

        /* B-TREE V2 HEADER */
        /* Removed: hdr_size, node_refc, file_refc, sizeof_size, sizeof_addr */

        /* Tracking */
        uint64_t                addr;          // addr of btree
        void                    *parent;       // potentially remove
        uint8_t                 max_nrec_size; // size to store max. # of records in any node (in bytes)

        /* Properties */
        btree2_subid_t          type;          // "class" H5B2_class_t under hdf5 title
        size_t                  nrec_size;     // native record size

        uint32_t                node_size;     // size in bytes of all B-tree nodes
        uint16_t                rrec_size;     // size in bytes of the B-tree record
        uint16_t                depth;
        uint8_t                 split_percent; // percent full that a node needs to increase above before it is split
        uint8_t                 merge_percent; // percent full that a node needs to be decrease below before it is split

        vector<btree2_node_info_t> node_info;  // table of node info structs for current depth of B-tree
        btree2_node_ptr_t       root;          // root struct
        vector<size_t>          nat_off;
        uint64_t                check_sum;
        dtable_t                dtable;        // doubling table

        /* UDATA */
        /* Removed: flags (attr storage location) and corder (creation order) */
        uint64_t                fheap_addr;
        H5Dataset::heap_info_t *fheap_info; // fractalH pointer
        const char              *name;         // attr name we are searching for
        uint32_t                name_hash;     // hash of attr name

        size_t                  sz_max_nrec;   // tmp variable for range checking
        uint32_t                u_max_nrec_size; // tmp variable for range checking

};