/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* MODIFICATIONS 
 *
 * This code adapts the HDF5 H5A__dense_open reading mechanism into C++.
 * 
 * SUPPORTED SEARCH / STRUCTURES:
 * - ONLY Type 8 Records - "Attribute Name for Indexed Attributes"
 * - ONLY Managed objects in the Fractal Heap
 * 
 * RELEVANT HD5 DOCUMENTATION: 
 * https://docs.hdfgroup.org/hdf5/v1_10/_f_m_t3.html#Btrees:~:text=III.A.2.%20Disk%20Format%3A%20Level%201A2%20%2D%20Version%202%20B%2Dtrees
 * 
 * CONTROL FLOW:
 * Externally, if regular attribute message reading fails in H5coro to locate the object,
 * H5Dense generates a temporary H5BTreeV2 object representing a Version 2 B-Tree.
 * V2 B-Tree contains a root node, which links to Internal Nodes (points to child nodes) 
 * and Leaf Nodes (contains Records). Nodes are organized under a Binary Search Tree (BST) 
 * style; the value used for the search is the "Hash of Name" in the Type 8 record. 
 * Once a matching "Hash of Name" is found for the desired attribute name, the Heap ID
 * is extracted from the record to search the fractal heap for the actual attribute message. 
 * An object in the fractal heap is identified by means of a heap ID.
 * 
 * The fractal heap stores an object in one of three ways, depending on the object’s size:
 * Managed, Tiny, Huge. Managed (Supported) uses a Doubling Table structure which uses the heap ID
 * to identify at what offset the object is located at.
 * 
 * Finally, the H5Dense returns control flow to the H5Coro progra via the H5BTreeV2 structure
 * On success, the structure provides the address of the Attribute Message.
 * 
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5Coro.h"
#include "core.h"

/******************************************************************************
 * H5 BTREE V2 CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5BTreeV2::H5BTreeV2(uint64_t fheap_addr, uint64_t name_bt2_addr, const char *name,  H5FileBuffer::heap_info_t* heap_info_ptr, H5FileBuffer* h5file)
{
    // TODO: init list; sequnece of init before body entered 
    found_out = false;
    h5filePtr_ = h5file;
    readDenseAttrs(fheap_addr, name_bt2_addr, name, heap_info_ptr);
}

/*----------------------------------------------------------------------------
 * readDenseAttributes
 *----------------------------------------------------------------------------*/
void H5BTreeV2::readDenseAttrs(uint64_t fheap_addr, uint64_t name_bt2_addr, const char *name, H5FileBuffer::heap_info_t* heap_info_ptr) {
    /* Open an attribute in dense storage structures for an object*/
    /* See hdf5 source implementation https://github.com/HDFGroup/hdf5/blob/45ac12e6b660edfb312110d4e3b4c6970ff0585a/src/H5Adense.c#L322 */
    
    /* TODO: shared Attr Support */
    print2term("WARNING: isTypeSharedAttrs is NOT implemented for dense attr reading \n");
    bool shared_attributes = isTypeSharedAttrs(H5FileBuffer::ATTRIBUTE_MSG);
    if (shared_attributes)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "sharedAttribute reading is not implemented");
    }

    btree2_ud_common_t udata; // struct to pass useful info e.g. name, hash
    bool attr_exists = false;

    btree2_hdr_t bt2_hdr;
    btree2_node_ptr_t root_node_ptr;
    dtable_t dtable;

    bt2_hdr.node_size = 0;

    /* Set udata - passed to findBTree interally */
    udata.fheap_addr = fheap_addr;
    udata.fheap_info = heap_info_ptr;
    udata.name = name;
    udata.name_hash = checksumLookup3(name, strlen(name), 0);
    udata.flags = 0;
    udata.corder = 0;

    /* openBTreeV2 - populates header info and allocates btree space */
    openBTreeV2(&bt2_hdr, &root_node_ptr, name_bt2_addr, heap_info_ptr, &udata, &attr_exists, &dtable);

    if (!attr_exists) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "FAILED to locate attribute with dense btreeV2 reading");
    }

}

 /*----------------------------------------------------------------------------
 * isTypeSharedAttrs
 *----------------------------------------------------------------------------*/
bool H5BTreeV2::isTypeSharedAttrs (unsigned type_id) {
    /* Equiv to H5SM_type_shared of HDF5 SRC Lib: https://github.com/HDFGroup/hdf5/blob/develop/src/H5SM.c#L328 */
    print2term("TODO: isTypeSharedAttrs implementation, omit support for v2 btree, recieved type_id %u \n", type_id);
    return false;
}

/*----------------------------------------------------------------------------
 * log2_gen - helper determines the log base two of a number
 *----------------------------------------------------------------------------*/
unsigned H5BTreeV2::log2_gen(uint64_t n) {
    /* Taken from https://github.com/HDFGroup/hdf5/blob/develop/src/H5VMprivate.h#L357 */
    unsigned r; // r will be log2(n)
    unsigned int t, tt, ttt; // temporaries

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

    if ((ttt = (unsigned)(n >> 32)))
        if ((tt = (unsigned)(n >> 48)))
            r = (t = (unsigned)(n >> 56)) ? 56 + (unsigned)LogTable256[t] : 48 + (unsigned)LogTable256[tt & 0xFF];
        else
            r = (t = (unsigned)(n >> 40)) ? 40 + (unsigned)LogTable256[t] : 32 + (unsigned)LogTable256[ttt & 0xFF];
    else if ((tt = (unsigned)(n >> 16)))
        r = (t = (unsigned)(n >> 24)) ? 24 + (unsigned)LogTable256[t] : 16 + (unsigned)LogTable256[tt & 0xFF];
    else
        // added 'uint8_t' cast to pacify PGCC compiler 
        r = (t = (unsigned)(n >> 8)) ? 8 + (unsigned)LogTable256[t] : (unsigned)LogTable256[(uint8_t)n];

    return (r);

}

/*----------------------------------------------------------------------------
 * log2of2 - helper replicating H5VM_log2_of2
 *----------------------------------------------------------------------------*/
unsigned H5BTreeV2::log2_of2(uint32_t n) {
    assert((!(n & (n - 1)) && n));

    static constexpr const unsigned MultiplyDeBruijnBitPosition[32] = {
        0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
        15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
        16, 7,  26, 12, 18, 6,  11, 5,  10, 9
    };

    return (unsigned)(MultiplyDeBruijnBitPosition[(n * (uint32_t)0x077CB531UL) >> 27]);
}

/*----------------------------------------------------------------------------
 * H5HF_SIZEOF_OFFSET_BITS
 *----------------------------------------------------------------------------*/
uint16_t H5BTreeV2::H5HF_SIZEOF_OFFSET_BITS(uint16_t b) {
    /* Compute the # of bytes required to store an offset into a given buffer size - taken from h5 macro */
    return (((b) + 7) / 8);
}


/*----------------------------------------------------------------------------
 * H5HF_SIZEOF_OFFSET_LEN
 *----------------------------------------------------------------------------*/
uint16_t H5BTreeV2::H5HF_SIZEOF_OFFSET_LEN(int l) {
    /* Offset Len spinning off bit size - taken from h5 macro */
    return H5HF_SIZEOF_OFFSET_BITS((uint16_t) log2_of2((unsigned) l));

}

/*----------------------------------------------------------------------------
 * H5_lookup3_rot
 *----------------------------------------------------------------------------*/
uint32_t H5BTreeV2::H5_lookup3_rot(uint32_t x, uint32_t k) {
    // #define H5_lookup3_rot(x, k) (((x) << (k)) ^ ((x) >> (32 - (k))))
    return (((x) << (k)) ^ ((x) >> (32 - (k))));

}

/*----------------------------------------------------------------------------
 * H5_lookup3_mix
 *----------------------------------------------------------------------------*/
void H5BTreeV2::H5_lookup3_mix(uint32_t& a,uint32_t& b, uint32_t& c) {
    a -= c;                                                                                              
    a ^= H5_lookup3_rot(c, 4);                                                                           
    c += b;                                                                                              
    b -= a;                                                                                              
    b ^= H5_lookup3_rot(a, 6);                                                                           
    a += c;                                                                                              
    c -= b;                                                                                              
    c ^= H5_lookup3_rot(b, 8);                                                                           
    b += a;                                                                                              
    a -= c;                                                                                              
    a ^= H5_lookup3_rot(c, 16);                                                                          
    c += b;                                                                                              
    b -= a;                                                                                              
    b ^= H5_lookup3_rot(a, 19);                                                                          
    a += c;                                                                                              
    c -= b;                                                                                              
    c ^= H5_lookup3_rot(b, 4);                                                                           
    b += a;
}

/*----------------------------------------------------------------------------
 * H5_lookup3_final
 *----------------------------------------------------------------------------*/
void H5BTreeV2::H5_lookup3_final(uint32_t& a, uint32_t& b, uint32_t& c) {
    c ^= b;                                                                                              
    c -= H5_lookup3_rot(b, 14);                                                                          
    a ^= c;                                                                                              
    a -= H5_lookup3_rot(c, 11);                                                                          
    b ^= a;                                                                                              
    b -= H5_lookup3_rot(a, 25);                                                                          
    c ^= b;                                                                                              
    c -= H5_lookup3_rot(b, 16);                                                                          
    a ^= c;                                                                                              
    a -= H5_lookup3_rot(c, 4);                                                                           
    b ^= a;                                                                                              
    b -= H5_lookup3_rot(a, 14);                                                                          
    c ^= b;                                                                                              
    c -= H5_lookup3_rot(b, 24);

}

/*----------------------------------------------------------------------------
 * checksumLookup3 - helper for jenkins hash
 *----------------------------------------------------------------------------*/

uint32_t H5BTreeV2::checksumLookup3(const void *key, size_t length, uint32_t initval) {
    /* Source: https://github.com/HDFGroup/hdf5/blob/develop/src/H5checksum.c#L365 */

    /* Initialize set up */
    const uint8_t *k = (const uint8_t *)key;
    uint32_t a, b, c = 0; 

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12) {
        a += k[0];
        a += ((uint32_t)k[1]) << 8;
        a += ((uint32_t)k[2]) << 16;
        a += ((uint32_t)k[3]) << 24;
        b += k[4];
        b += ((uint32_t)k[5]) << 8;
        b += ((uint32_t)k[6]) << 16;
        b += ((uint32_t)k[7]) << 24;
        c += k[8];
        c += ((uint32_t)k[9]) << 8;
        c += ((uint32_t)k[10]) << 16;
        c += ((uint32_t)k[11]) << 24;
        H5_lookup3_mix(a, b, c);
        length -= 12;
        k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length) /* all the case statements fall through */
    {
        case 12:
            c += ((uint32_t)k[11]) << 24;
        case 11:
            c += ((uint32_t)k[10]) << 16;
        case 10:
            c += ((uint32_t)k[9]) << 8;
        case 9:
            c += k[8];
        case 8:
            b += ((uint32_t)k[7]) << 24;
        case 7:
            b += ((uint32_t)k[6]) << 16;
        case 6:
            b += ((uint32_t)k[5]) << 8;
        case 5:
            b += k[4];
        case 4:
            a += ((uint32_t)k[3]) << 24;
        case 3:
            a += ((uint32_t)k[2]) << 16;
        case 2:
            a += ((uint32_t)k[1]) << 8;
        case 1:
            a += k[0];
            break;
        case 0:
            return c;
        default:
            assert(0 && "This should never be executed!");
    }

    H5_lookup3_final(a, b, c);

    return c;

}

/*----------------------------------------------------------------------------
 * checkAssigned - verify that value doesn't overflow when casted to type T
 *----------------------------------------------------------------------------*/
template<typename T, typename V>
bool H5BTreeV2::checkAssigned(const T& type, const V& value) {
    print2term("Recieved type for assignment check: %u", (unsigned) type);
    return value <= std::numeric_limits<T>::max();
}

 /*----------------------------------------------------------------------------
 * addrDecode - helper
 *----------------------------------------------------------------------------*/
void H5BTreeV2::addrDecode(size_t addr_len, const uint8_t **pp, uint64_t* addr_p) {
    // decode from buffer at **pp (aka pos), out into addr_p
    // updates the pointer to point to the next byte after the address 
    // see H5F_addr_decode_len

    bool all_zero = true;
    unsigned u;

    /* Reset value in destination */
    *addr_p = 0;

    /* Decode bytes from address */
    for (u = 0; u < addr_len; u++) {
        uint8_t c; /* Local decoded byte */

        /* Get decoded byte (and advance pointer) */
        c = *(*pp)++;

        /* Check for non-undefined address byte value */
        if (c != 0xff)
            all_zero = false;

        if (u < sizeof(*addr_p)) {
            uint64_t tmp = c; /* Local copy of address, for casting */

            /* Shift decoded byte to correct position */
            tmp <<= (u * 8); /*use tmp to get casting right */

            /* Merge into already decoded bytes */
            *addr_p |= tmp;
        } /* end if */
        else if (!all_zero)
            assert(0 == **pp); /*overflow */
    }                          /* end for */

    /* If 'all_zero' is still true, the address was entirely composed of '0xff'
     *  bytes, which is the encoded form of 'HADDR_UNDEF', so set the destination
     *  to that value */
    if (all_zero)
        *addr_p = UINT64_MAX; // TODO: this macro may not be imported; use #include <cstdint> if not

}


void H5BTreeV2::varDecode(uint8_t* p, int n, uint8_t l) {
    /* Taken from DECODE_VAR https://github.com/mannau/h5-libwin/blob/24f3884b145417299810f9ec16c41abc92d850df/include/hdf5/H5Fprivate.h#L145 */
    /* ARG TYPES TAKEN FROM REFERENCE HERE: https://github.com/HDFGroup/hdf5/blob/49cce9173f6e43ffda2924648d863dcb4d636993/src/H5B2cache.c#L668 */
    /* Decode a variable-sized buffer */
    /* (Assumes that the high bits of the integer will be zero) */
    
    size_t _i;
    n = 0;
    (*p) += l; // (p) += l;
    for (_i = 0; _i < l; _i++) {
        n = (n << 8) | *(--p);
    }
    (*p) += l; // (p) += l;
}

/*----------------------------------------------------------------------------
 * decodeType5Record - Group Decoding
 *----------------------------------------------------------------------------*/
void H5BTreeV2::decodeType5Record(const uint8_t *raw, void *_nrecord) {
    /* Implementation of H5G__dense_btree2_name_decode */
    // https://github.com/HDFGroup/hdf5/blob/49cce9173f6e43ffda2924648d863dcb4d636993/src/H5Gbtree2.c#L286

    /* TODO: fix reading fields, DO NOT USE DECODE MACRO */
    btree2_type5_densename_rec_t *nrecord = (btree2_type5_densename_rec_t *)_nrecord;
    size_t H5G_DENSE_FHEAP_ID_LEN = 7;
    // TODO FIX
    raw += 4;
    memcpy(nrecord->id, raw, H5G_DENSE_FHEAP_ID_LEN);
}

/*----------------------------------------------------------------------------
 * decodeType8Record
 *----------------------------------------------------------------------------*/
uint64_t H5BTreeV2::decodeType8Record(uint64_t internal_pos, void *_nrecord) {
    /* Decode Version 2 B-tree, Type 8 Record*/
    /* See HDF5 official documentation: https://docs.hdfgroup.org/hdf5/v1_10/_f_m_t3.html#DatatypeMessage:~:text=Layout%3A%20Version%202%20B%2Dtree%2C%20Type%208%20Record%20Layout%20%2D%20Attribute%20Name%20for%20Indexed%20Attributes */

    unsigned u = 0;
    btree2_type8_densename_rec_t *nrecord = (btree2_type8_densename_rec_t *)_nrecord;
    
    for (u = 0; u < H5O_FHEAP_ID_LEN; u++ ) {
        nrecord->id.id[u] = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    }

    nrecord->flags = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    nrecord->corder = (uint32_t) h5filePtr_->readField(4, &internal_pos);
    nrecord->hash = (uint32_t) h5filePtr_->readField(4, &internal_pos);

    return internal_pos;
    
}

/*----------------------------------------------------------------------------
 * fheapLocate
 *----------------------------------------------------------------------------*/
void H5BTreeV2::fheapLocate(btree2_hdr_t* hdr_og, H5FileBuffer::heap_info_t *hdr, const void * _id) {

    /* Dispatcher for heap ID types - currently only supporting manual type */
    uint8_t* id = (uint8_t*)_id;
    uint8_t id_flags = 0; // heap ID flag bits 
    id_flags = *id;

    if ((id_flags & H5HF_ID_VERS_MASK) != H5HF_ID_VERS_CURR) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Incorrect heap ID version");
    }

    /* Check type of object in heap */
    if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_MAN) {
        /* Operate on object from managed heap blocks */
        fheapLocate_Managed(hdr_og, hdr, id);
    } 
    else if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_HUGE) {
        /* NOT IMPLEMENTED - Operate on 'huge' object from file */
        // fheapLocate_Huge();
        throw RunTimeException(CRITICAL, RTE_ERROR, "Huge heap ID reading not supported");
    } 
    else if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_TINY) {
        /* NOT IMPLEMENTED - Operate on 'tiny' object from file */
        // fheapLocate_Tiny();
        throw RunTimeException(CRITICAL, RTE_ERROR, "Tiny heap ID reading not supported");
    } 
    else {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unsupported Heap ID");
    }

}

/*----------------------------------------------------------------------------
 * dtableLookup
 *----------------------------------------------------------------------------*/
void H5BTreeV2::dtableLookup(H5FileBuffer::heap_info_t* hdr, dtable_t* dtable, uint64_t off, unsigned *row, unsigned *col) {

    bool checkA = false;

    /* Check for offset in first row */
    if (off < dtable->num_id_first_row) {
        *row = 0;
        checkA = checkAssigned(*col, off / hdr->starting_blk_size);
        assert(checkA && "Failed checkAssigned for dtableLookup"); 
        *col = (unsigned) (off / hdr->starting_blk_size);
    }
    else {
        unsigned high_bit = log2_gen(off); // determine the high bit in the offset
        uint64_t off_mask = ((uint64_t)1) << high_bit; // compute mask for determining column

        *row = (high_bit - dtable->first_row_bits) + 1;
        checkA = checkAssigned(*col, ((off - off_mask) / dtable->row_block_size[*row]));
        assert(checkA && "Failed checkAssigned for dtableLookup"); 
        *col = ((off - off_mask) / dtable->row_block_size[*row]);
    } 

}

/*----------------------------------------------------------------------------
 * buildEntries_Indirect
 *----------------------------------------------------------------------------*/
uint64_t H5BTreeV2::buildEntries_Indirect(H5FileBuffer::heap_info_t* heap_info, int nrows, uint64_t pos, uint64_t* ents) {
    /* Build array of addresses (ent) for this indirect block - follow H5HF__cache_iblock_deserialize x readIndirectBlock */
    unsigned idx = 0; // track indx in ents
    uint64_t block_off = 0; // iblock->block_off for moving offset
    
    pos += 5; // signature and version
    pos += h5filePtr_->metaData.offsetsize; // skip block header

    /* Build equivalent of iblock->block_off - copy indirect style*/
    const int MAX_BLOCK_OFFSET_SIZE = 8;
    uint8_t block_offset_buf[MAX_BLOCK_OFFSET_SIZE] = {0};
    h5filePtr_->readByteArray(block_offset_buf, heap_info->blk_offset_size, &pos); // Block Offset
    memcpy(&block_off, block_offset_buf, sizeof(uint64_t));

    // TODO: this has no regard for separating types --> valid approach?
    // NOTE: when debugging compare entries encoutered
    for(int row = 0; row < nrows; row++)
    {
        /* Calculate Row's Block Size */
        int row_block_size;
        if (row == 0) {
            row_block_size = heap_info->starting_blk_size;
        }
        else if (row == 1) { 
            row_block_size = heap_info->starting_blk_size;
        }
        else {
            row_block_size = heap_info->starting_blk_size * (0x2 << (row - 2));
        }

        /* Process Entries in Row */
        for(int entry = 0; entry < heap_info->table_width; entry++)
        {
            /* Direct Block Entry */
            if(row_block_size <= heap_info->max_dblk_size)
            {
                /* Read Direct Block Address and Assign */
                uint64_t direct_block_addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
                ents[idx] = direct_block_addr;
                idx++;
                
            }
            else /* Indirect Block Entry and Assign */
            {
                /* Read Indirect Block Address */
                uint64_t indirect_block_addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
                ents[idx] = indirect_block_addr;
                idx++;
            }
        }
    }

    return block_off;

}

/*----------------------------------------------------------------------------
 * man_dblockLocate
 *----------------------------------------------------------------------------*/
void H5BTreeV2::man_dblockLocate(btree2_hdr_t* hdr_og, H5FileBuffer::heap_info_t* hdr, uint64_t obj_off, uint64_t* ents, unsigned *ret_entry) {
    /* Mock implementation of H5HF__man_dblock_locate */
    /* This method should only be called if we can't directly readDirect */
    // iblock->ents <-- ents derived from where H5HF_indirect_t **ret_iblock passed

    uint64_t iblock_addr = 0; 
    unsigned row = 0, col = 0;
    uint64_t block_off = 0;

    /* Look up row & column for object */
    dtableLookup(hdr, hdr_og->dtable, obj_off, &row, &col);

    /* Set indirect */
    iblock_addr = hdr_og->dtable->table_addr;
    
    /* Read indirect - set up ents array */
    int nrows = hdr->curr_num_rows;
    block_off = buildEntries_Indirect(hdr, nrows, iblock_addr, ents);

    /* Check for indirect block row - iterates till direct hit */
    while (row >= hdr_og->dtable->max_direct_rows) {
        unsigned entry;
        /* Compute # of rows in child indirect block */
        nrows = (log2_gen(hdr_og->dtable->row_block_size[row]) - hdr_og->dtable->first_row_bits) + 1;
        entry = (row * (unsigned)hdr->table_width) + col;
        /* Locate child indirect block */
        iblock_addr = ents[entry];
        /* new iblock search and set - row col - remove offset worht of iblock*/
        dtableLookup(hdr, hdr_og->dtable, (obj_off - block_off), &row, &col);
        /* Switch to new block - new ents list */
        block_off = buildEntries_Indirect(hdr, nrows, iblock_addr, ents);

    }

    /* populate entry pointer using derived info */
    *ret_entry = (row * (unsigned)hdr->table_width) + col;

}

/*----------------------------------------------------------------------------
 * fheapLocate_Managed
 *----------------------------------------------------------------------------*/
void H5BTreeV2::fheapLocate_Managed(btree2_hdr_t* hdr_og, H5FileBuffer::heap_info_t* hdr, uint8_t* id){
    /* Operate on managed heap - eqiv to hdf5 ref functions: H5HF__man_op, H5HF__man_op_real*/

    uint64_t dblock_addr = 0; // found direct block to apply offset on
    uint64_t dblock_block_off = 0; // dblock block offset, extracted from top of dir block header
    uint64_t obj_off = 0; // offset of object in heap 
    size_t obj_len = 0; // len of object in heap
    size_t blk_off = 0; // offset of object in block 
    
    id++; // skip due to flag reading
    memcpy(&obj_off, id, (size_t)(hdr->heap_off_size));
    id += hdr->heap_off_size;
    memcpy(&obj_len, id, (size_t)(hdr->heap_len_size));

    /* Locate direct block of interest */
    if(hdr_og->dtable->curr_root_rows == 0)
    {
        /* Direct Block - set address of block before deploying down readDirectBlock */
        dblock_addr = hdr_og->dtable->table_addr;
    }
    else
    {
        /* Indirect Block Navigation */
        uint64_t ents[(size_t)hdr->curr_num_rows * hdr->table_width]; // mimic H5HF_indirect_ent_t of the H5HF_indirect_t struct
        unsigned entry; // entry of block
        
        /* Search for direct block using double table and offset */
        man_dblockLocate(hdr_og, hdr, obj_off, ents, &entry); 
        dblock_addr = ents[entry];

    }

    /* read direct block to access message */
    uint64_t pos = dblock_addr;
    pos += 5; // skip signature and version of object 
    pos += h5filePtr_->metaData.offsetsize; // skip heap hdr addr

    /* Unpack block offset */
    const int MAX_BLOCK_OFFSET_SIZE = 8;
    uint8_t new_block_offset_buf[MAX_BLOCK_OFFSET_SIZE] = {0};
    h5filePtr_->readByteArray(new_block_offset_buf, hdr->blk_offset_size, &pos);
    memcpy(&dblock_block_off, new_block_offset_buf, sizeof(uint64_t));

    // TODO: checksum (only present if flags 1 see spec)

    /* position pointer inside of dblock to read object */
    blk_off = (size_t)(obj_off - dblock_block_off);
    pos = dblock_addr + (uint64_t) blk_off;
    uint64_t msg_size = (uint64_t) obj_len;

    /* read object switch on mssg type */
    switch(hdr_og->type) {
        case H5B2_ATTR_DENSE_NAME_ID:
            /* return info for outside readAttributeMessage - complete obj construction*/
            pos_out = pos;
            hdr_flags_out = hdr->hdr_flags;
            hdr_dlvl_out = hdr->dlvl;
            msg_size_out = msg_size;
            break;
        default:
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented hdr->type message read: %d", hdr_og->type);
    }

}

/*----------------------------------------------------------------------------
 * fheapNameCmp
 *----------------------------------------------------------------------------*/
void H5BTreeV2::fheapNameCmp(const void *obj, size_t obj_len, void *op_data){

    // temp satisfy print
    print2term("fheapNameCmp args: %lu, %lu, %lu", (uintptr_t) obj, (uintptr_t) obj_len, (uintptr_t) op_data);
    // TODO
    return;

}

 /*----------------------------------------------------------------------------
 * compareType8Record
 *----------------------------------------------------------------------------*/
void H5BTreeV2::compareType8Record(btree2_hdr_t* hdr, const void *_bt2_udata, const void *_bt2_rec, int *result)
{
    /* Implementation of H5A__dense_btree2_name_compare with type 8 - H5B2_GRP_DENSE_NAME_ID*/
    /* See: https://github.com/HDFGroup/hdf5/blob/0ee99a66560422fc20864236a83bdcd0103d8f64/src/H5Abtree2.c#L220 */

    const btree2_ud_common_t *bt2_udata = (const btree2_ud_common_t *)_bt2_udata;
    const btree2_type8_densename_rec_t *bt2_rec   = (const btree2_type8_densename_rec_t *)_bt2_rec;

    /* Check hash value - influence btree search direction */
    if (bt2_udata->name_hash < bt2_rec->hash)
        *result = (-1);
    else if (bt2_udata->name_hash > bt2_rec->hash)
        *result = 1;
    else {
        /* Hash match */
        H5FileBuffer::heap_info_t *fheap; // equiv to: pointer to internal fractal heap header info

        /* Sanity check */
        assert(bt2_udata->name_hash == bt2_rec->hash);

        if (bt2_rec->flags & H5O_MSG_FLAG_SHARED) {
            // TODO: shared fractal heap support, see OG implementation; fheap = bt2_udata->shared_fheap;
            throw RunTimeException(CRITICAL, RTE_ERROR, "No support implemented for shared fractal heaps");
        }
        else {
            fheap = bt2_udata->fheap_info;
        }

        /* Locate object in fractal heap */
        fheapLocate(hdr, fheap, &bt2_rec->id);
        *result = 0;
        
    } 
}

 /*----------------------------------------------------------------------------
 * locateRecordBTreeV2
 *----------------------------------------------------------------------------*/
void H5BTreeV2::locateRecordBTreeV2(btree2_hdr_t* hdr, unsigned nrec, size_t *rec_off, const uint8_t *native, const void *udata, unsigned *idx, int *cmp) {
    /* Performs a binary search to locate a record in a sorted array of records 
    sets *idx to location of record greater than or equal to record to locate */
    /* hdf5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2int.c#L89 */

    unsigned lo = 0, hi = nrec; // bounds of search range
    unsigned my_idx = 0; // current record idx

    *cmp = -1;

    while (lo < hi && *cmp) {
        /* call on type compare method */
        my_idx = (lo + hi) / 2;
        switch(hdr->type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                compareType8Record(hdr, udata, native + rec_off[my_idx], cmp);
                break;
            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented hdr->type compare: %d", hdr->type);
        }
        if (*cmp < 0)
            hi = my_idx;
        else
            lo = my_idx + 1;
    }

    /* negative for record to locate less than *idx val, zero if equal, positive if greater than */
    *idx = my_idx;
}


 /*----------------------------------------------------------------------------
 * initHdrBTreeV2
 *----------------------------------------------------------------------------*/
void H5BTreeV2::initHdrBTreeV2(btree2_hdr_t *hdr, uint64_t addr, btree2_node_ptr_t *root_node_ptr) {
    
    /* populate header */
    hdr->addr = addr;
    uint64_t pos = addr;
    uint32_t signature = (uint32_t)h5filePtr_->readField(4, &pos);

    /* Signature check */
    if(signature != H5FileBuffer::H5_V2TREE_SIGNATURE_LE)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid btree header signature: 0x%llX", (unsigned long long)signature);
    }
    /* Version check */
    uint8_t version = (uint8_t) h5filePtr_->readField(1, &pos);
    if(version != 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid btree header version: %hhu", version);
    }

    /* Record type extraction */
    hdr->type = (btree2_subid_t) h5filePtr_->readField(1, &pos);

    /* Cont. parse Btreev2 header */
    hdr->node_size = (uint32_t) h5filePtr_->readField(4, &pos);
    hdr->rrec_size = (uint16_t) h5filePtr_->readField(2, &pos);
    hdr->depth = (uint16_t) h5filePtr_->readField(2, &pos);
    hdr->split_percent = (uint8_t) h5filePtr_->readField(1, &pos);
    hdr->merge_percent = (uint8_t) h5filePtr_->readField(1, &pos);
    root_node_ptr->addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
    root_node_ptr->node_nrec = (uint16_t)h5filePtr_->readField(2, &pos);
    root_node_ptr->all_nrec = h5filePtr_->readField(h5filePtr_->metaData.lengthsize, &pos);
    hdr->root = root_node_ptr;
    hdr->check_sum = h5filePtr_->readField(4, &pos);

    /* allocate native record size using type found */
    switch(hdr->type) {
        case H5B2_ATTR_DENSE_NAME_ID:
            hdr->nrec_size = sizeof(btree2_type8_densename_rec_t);
            break;
        default:
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented type for nrec_size: %d", hdr->type);
    }

    if (hdr->node_size == 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "0 return in bt2_hdr->node_size, failure suspect in openBtreeV2");
    }
}

 /*----------------------------------------------------------------------------
 * initDTable
 *----------------------------------------------------------------------------*/
void H5BTreeV2::initDTable(btree2_hdr_t *hdr, H5FileBuffer::heap_info_t* heap_info_ptr, dtable_t* dt_curr, vector<uint64_t>& row_block_size, vector<uint64_t>& row_block_off, vector<uint64_t>& row_tot_dblock_free, vector<uint64_t>& row_max_dblock_free) {
    /* table_init HDF5 reference implementation: https://github.com/HDFGroup/hdf5/blob/02a57328f743342a7e26d47da4aac445ee248782/src/H5HFdtable.c#L75 */

    /* main dtable */
    hdr->dtable = dt_curr;

    /* offsets and sizes */
    hdr->dtable->start_bits = log2_of2((uint32_t)heap_info_ptr->starting_blk_size);
    hdr->dtable->first_row_bits = hdr->dtable->start_bits + log2_of2((uint32_t)heap_info_ptr->table_width);
    hdr->dtable->max_root_rows = (heap_info_ptr->max_heap_size - hdr->dtable->first_row_bits) + 1;
    hdr->dtable->max_direct_bits = log2_of2((uint32_t)heap_info_ptr->max_dblk_size);
    hdr->dtable->max_direct_rows = (hdr->dtable->max_direct_bits - hdr->dtable->start_bits) + 2;
    hdr->dtable->num_id_first_row = (uint64_t) (heap_info_ptr->starting_blk_size * heap_info_ptr->table_width);
    hdr->dtable->max_dir_blk_off_size = H5HF_SIZEOF_OFFSET_LEN(heap_info_ptr->max_dblk_size);
    hdr->dtable->curr_root_rows = (unsigned) heap_info_ptr->curr_num_rows;
    hdr->dtable->table_addr = heap_info_ptr->root_blk_addr;

    row_block_size.resize(hdr->dtable->max_root_rows);
    row_block_off.resize(hdr->dtable->max_root_rows);
    row_tot_dblock_free.resize(hdr->dtable->max_root_rows);
    row_max_dblock_free.resize(hdr->dtable->max_root_rows);

    hdr->dtable->row_block_size = row_block_size;
    hdr->dtable->row_block_off = row_block_off;
    hdr->dtable->row_tot_dblock_free = row_tot_dblock_free;
    hdr->dtable->row_max_dblock_free = row_max_dblock_free;

    /* buid block sizing for each row*/
    uint64_t tmp_block_size = (uint64_t) heap_info_ptr->starting_blk_size;
    uint64_t acc_block_off = (uint64_t) heap_info_ptr->starting_blk_size * heap_info_ptr->table_width;
    hdr->dtable->row_block_size[0] = (uint64_t) heap_info_ptr->starting_blk_size;
    hdr->dtable->row_block_off[0] = 0;
    
    for (size_t j = 1; j < hdr->dtable->max_root_rows; j++) {
        hdr->dtable->row_block_size[j] = tmp_block_size;
        hdr->dtable->row_block_off[j] = acc_block_off;
        tmp_block_size *= 2;
        acc_block_off *= 2;
    }
}

 /*----------------------------------------------------------------------------
 * initNodeInfo
 *----------------------------------------------------------------------------*/
void H5BTreeV2::initNodeInfo(btree2_hdr_t *hdr, vector<btree2_node_info_t>& node_info, vector<size_t>& nat_off) {

    /* main node info */
    node_info.resize(hdr->depth + 1);
    hdr->node_info = node_info;

    /* Init leaf node info */
    size_t sz_max_nrec = (((hdr->node_size) - H5B2_METADATA_PREFIX_SIZE) / (hdr->rrec_size));  

    bool checkA = checkAssigned(hdr->node_info[0].max_nrec, sz_max_nrec);
    assert(checkA && "Failed checkAssigned for initNodeInfo: sz_max_nrec overflows type of of hdr->node_info[0].max_nrec"); 
    hdr->node_info[0].max_nrec = (unsigned) sz_max_nrec;

    hdr->node_info[0].split_nrec = (hdr->node_info[0].max_nrec * hdr->split_percent) / 100;
    hdr->node_info[0].merge_nrec = (hdr->node_info[0].max_nrec * hdr->merge_percent) / 100;
    hdr->node_info[0].cum_max_nrec = hdr->node_info[0].max_nrec;
    hdr->node_info[0].cum_max_nrec_size = 0;

    /* alloc array of pointers to internal node native keys */
    nat_off.resize(hdr->node_info[0].max_nrec);
    hdr->nat_off = nat_off;
    
    /* Initialize offsets in native key block */
    if (hdr->node_info[0].max_nrec != 0 ){
        for (unsigned u = 0; u < hdr->node_info[0].max_nrec; u++) {
            hdr->nat_off[u] = hdr->nrec_size * u;
        }
    }
    else {
        throw RunTimeException(CRITICAL, RTE_ERROR, "critical failure");
    }

    /* Compute size to compute num records in each record */
    unsigned u_max_nrec_size = (log2_gen((uint64_t)hdr->node_info[0].max_nrec) / 8) + 1;

    checkA = checkAssigned(hdr->max_nrec_size, u_max_nrec_size);
    assert(checkA && "Failed checkAssigned for initNodeInfo: u_max_nrec_size exceeds uint8 rep limit"); 
    hdr->max_nrec_size = (uint8_t) u_max_nrec_size;

    assert(hdr->max_nrec_size <= H5B2_SIZEOF_RECORDS_PER_NODE);

    /* Init internal node info, including size and number */
    if (hdr->depth > 0) {
        for (unsigned u = 1; u < (unsigned)(hdr->depth + 1); u++) {
            print2term("WARNING: UNTESTED IMPLEMENTATION FOR INTERNAL NODE \n");

            unsigned b2_int_ptr_size = (unsigned)(h5filePtr_->metaData.offsetsize) + hdr->max_nrec_size + (hdr->node_info[(u)-1]).cum_max_nrec_size; // = H5B2_INT_POINTER_SIZE(h, u) 
            sz_max_nrec = ((hdr->node_size - (H5B2_METADATA_PREFIX_SIZE + b2_int_ptr_size)) / (hdr->rrec_size + b2_int_ptr_size)); // = H5B2_NUM_INT_REC(hdr, u);
            checkA = checkAssigned(hdr->node_info[u].max_nrec, sz_max_nrec);
            assert(checkA && "Failed checkAssigned for initNodeInfo: sz_max_nrec exceeds unsigned rep limit");
            hdr->node_info[u].max_nrec = sz_max_nrec;

            assert(hdr->node_info[u].max_nrec <= hdr->node_info[u - 1].max_nrec);
            hdr->node_info[u].split_nrec = (hdr->node_info[u].max_nrec * hdr->split_percent) / 100;
            hdr->node_info[u].merge_nrec = (hdr->node_info[u].max_nrec * hdr->merge_percent) / 100;
            hdr->node_info[u].cum_max_nrec = ((hdr->node_info[u].max_nrec + 1) * hdr->node_info[u - 1].cum_max_nrec) + hdr->node_info[u].max_nrec;
            u_max_nrec_size = (log2_gen((uint64_t)hdr->node_info[u].cum_max_nrec) / 8) + 1;

            checkA = checkAssigned(hdr->node_info[u].cum_max_nrec_size, u_max_nrec_size);
            assert(checkA && "Failed checkAssigned for initNodeInfo: u_max_nrec_size exceeds uint8 rep limit");
            hdr->node_info[u].cum_max_nrec_size= (uint8_t) u_max_nrec_size;
        
        }
    }

}


 /*----------------------------------------------------------------------------
 * openBTreeV2
 *----------------------------------------------------------------------------*/
void H5BTreeV2::openBTreeV2 (btree2_hdr_t *hdr, btree2_node_ptr_t *root_node_ptr, uint64_t addr, H5FileBuffer::heap_info_t* heap_info_ptr, void* udata, bool* found, dtable_t* dt_curr) {
    /* Opens an existing v2 B-tree in the file; initiates: btreev2 header, double table, array of node info structs, pointers to internal nodes, internal node info, offsets */
    /* openBtree HDF5 reference implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2.c#L186 */
    /* header_init HDF5 reference implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2hdr.c#L94*/

    /* Stack inits */
    vector<btree2_node_info_t> node_info;
    vector<size_t> nat_off;
    vector<uint64_t> row_block_size;
    vector<uint64_t> row_block_off;
    vector<uint64_t> row_tot_dblock_free;
    vector<uint64_t> row_max_dblock_free;

    /* Btreev2 header */
    initHdrBTreeV2(hdr, addr, root_node_ptr);

    /* Node info structs */
    initNodeInfo(hdr, node_info, nat_off);

    /* Initiate Double Table */
    initDTable(hdr, heap_info_ptr, dt_curr, row_block_size, row_block_off, row_tot_dblock_free, row_max_dblock_free);

    /* Attempt to locate record with matching name */
    findBTreeV2(hdr, udata, found);
    
}

 /*----------------------------------------------------------------------------
 * openInternalNode
 *----------------------------------------------------------------------------*/
void H5BTreeV2::openInternalNode(btree2_internal_t *internal, btree2_hdr_t* hdr, uint64_t internal_pos, btree2_node_ptr_t* curr_node_ptr) {
    /* Set up internal node structure from given addr start: internal_pos */

    uint8_t *native = nullptr;  
    unsigned u = 0;
    int node_nrec = 0;

    /* Signature sanity check */
    uint32_t signature = (uint32_t) h5filePtr_->readField(4, &internal_pos);
    if (signature != H5FileBuffer::H5_V2TREE_INTERNAL_SIGNATURE_LE) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Signature does not match internal node: %u", signature);
    }

    /* Version check */
    uint8_t version = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if (signature != 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid version for internal node: %u", version);
    }

    /* B-tree Type check */
    uint8_t type = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if ((btree2_subid_t)type != hdr->type) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid type for internal node: %u, expected from hdr: %u", type, hdr->type);
    }

    /* Data Intake Prep -  H5B2__cache_int_deserialize */
    internal->hdr = hdr;
    internal->nrec = curr_node_ptr->node_nrec; // assume internal reflected in current ptr
    internal->depth = hdr->depth;
    internal->parent = hdr->parent;

    /* Deserialize records*/
    native = internal->int_native.data();
    for (u = 0; u < internal->nrec; u++) {

        /* Decode record via set decode method from type- modifies native arr directly */
        switch(hdr->type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                internal_pos = decodeType8Record(internal_pos, native);
                break;
            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented hdr->type for decode: %d", hdr->type);
        }

        /* Move to next record */
        native += hdr->nrec_size;

    } 
    
    /* Deserialize node pointers for internal node */
    btree2_node_ptr_t *int_node_ptr = internal->node_ptrs.data();
    for (u = 0; u < (unsigned)(internal->nrec + 1); u++) {
        /* Decode node address -- see H5F_addr_decode */
        size_t addr_size = (size_t) h5filePtr_->metaData.offsetsize; // as defined by hdf spec

        addrDecode(addr_size, (const uint8_t **)&internal_pos, &(int_node_ptr->addr)); // internal pos value should change
        varDecode((uint8_t *)internal_pos, node_nrec, hdr->max_nrec_size);

        bool checkA = checkAssigned(int_node_ptr->node_nrec, node_nrec);
        assert(checkA && "node_nrec exceeds uint16 rep limit");
        int_node_ptr->node_nrec = (uint16_t) node_nrec;

        if (internal->depth > 1) {
            varDecode((uint8_t *)internal_pos, int_node_ptr->all_nrec, hdr->node_info[internal->depth - 1].cum_max_nrec_size);
        }
        else {
            int_node_ptr->all_nrec = int_node_ptr->node_nrec;
        }

        /* Move to next node pointer */
        int_node_ptr++;

    }

    /* Metadata checksum */
    // TODO

    /* Return structure inside of internal */
    return;
}

/*----------------------------------------------------------------------------
 * openLeafNode
 *----------------------------------------------------------------------------*/
uint64_t H5BTreeV2::openLeafNode(btree2_hdr_t* hdr, btree2_node_ptr_t *curr_node_ptr, btree2_leaf_t *leaf, uint64_t internal_pos) {
    /* given pointer to lead node, set *leaf struct and deserialize the records contained at the node */
    /* hdf5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2cache.c#L988 */

    uint8_t *native = nullptr;  
    unsigned u = 0; 
    leaf->hdr = hdr;

    /* Signature Check*/
    uint32_t signature = (uint32_t) h5filePtr_->readField(4, &internal_pos);
    if (signature != H5FileBuffer::H5_V2TREE_LEAF_SIGNATURE_LE) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Signature does not match leaf node: %u", signature);
    }

    /* Version check */
    uint8_t version = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if (version != 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Version does not match leaf node: %u", version);
    }

    /* Type check */
    btree2_subid_t type = (btree2_subid_t) h5filePtr_->readField(1, &internal_pos);
    if (type != hdr->type) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Type of leaf node does not match header type: %u", type);
    }

    /* Allocate space for the native keys in memory & set num records */
    leaf->nrec = curr_node_ptr->node_nrec;
    
    /* Deserialize records*/
    native = leaf->leaf_native.data();
    for (u = 0; u < leaf->nrec; u++) {
        /* Type switching */
        switch(hdr->type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                internal_pos = decodeType8Record(internal_pos, native);
                break;
            
            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented hdr->type for decode: %d", hdr->type);
        }
        /* move to populate next record*/
        native += hdr->nrec_size;
    }

    // TODO: checksum verification

    /* Return final movement */
    return internal_pos;

}

 /*----------------------------------------------------------------------------
 * findBTreeV2
 *----------------------------------------------------------------------------*/
 void H5BTreeV2::findBTreeV2 (btree2_hdr_t* hdr, void* udata, bool *found) {
    /* Given start of btreev2, search for matching udata */
    /* HDF5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2.c#L429 */

    btree2_node_ptr_t* curr_node_ptr = nullptr; // node ptr info for curr
    uint16_t depth = 0;                         // of tree
    int cmp = 0;                          // comparison value of records (0 if found, else -1 or 1 for search direction)
    unsigned idx = 0;                     // location (index) of record which matches key
    btree2_nodepos_t curr_pos;        // address of curr ndoe

    /* Copy root ptr - exit if empty tree */
    curr_node_ptr = hdr->root;

    /* Sanity check: no records at node imply finished search */
    if (curr_node_ptr->node_nrec == 0) {
        *found = false;
        return;
    }

    /* Skip min/max accelerated search; note "SWMR writes" - TODO in future*/
    depth = hdr->depth;

    /* Walk down B-tree to find record or leaf node where record is located */
    cmp = -1;
    curr_pos = H5B2_POS_ROOT;
     
    /* Init internal */
    btree2_node_ptr_t next_node_ptr;
    btree2_internal_t internal;
    internal.int_native = vector<uint8_t>((size_t)(hdr->rrec_size * curr_node_ptr->node_nrec));
    internal.node_ptrs = vector<btree2_node_ptr_t>((unsigned)(curr_node_ptr->node_nrec + 1));

    while (depth > 0) {

        print2term("WARNING: INCOMPLETE IMPLEMENTATION FOR INTERNAL NODE \n");

        /* INTERNAL NODE SET UP - Write into internal */
        uint64_t internal_pos = curr_node_ptr->addr; // snapshot internal addr start
        openInternalNode(&internal, hdr, internal_pos, curr_node_ptr); // internal set with all info for locate record

        /* LOCATE RECORD - via type compare method */
        locateRecordBTreeV2(hdr, internal.nrec, hdr->nat_off.data(), internal.int_native.data(), udata, &idx, &cmp);

        if (cmp > 0) {
            idx++;
        }
        if (cmp != 0) {
            
            next_node_ptr = internal.node_ptrs[idx];

            /* Set the position of the next node */
            if (H5B2_POS_MIDDLE != curr_pos) {
                if (idx == 0) {
                    if (H5B2_POS_LEFT == curr_pos || H5B2_POS_ROOT == curr_pos)
                        curr_pos = H5B2_POS_LEFT;
                    else
                        curr_pos = H5B2_POS_MIDDLE;
                } /* end if */
                else if (idx == internal.nrec) {
                    if (H5B2_POS_RIGHT == curr_pos || H5B2_POS_ROOT == curr_pos)
                        curr_pos = H5B2_POS_RIGHT;
                    else
                        curr_pos = H5B2_POS_MIDDLE;
                } /* end if */
                else
                    curr_pos = H5B2_POS_MIDDLE;
            }

            /* Set pointer to next node to load */
            curr_node_ptr = &next_node_ptr;

        }
        else {
            *found = true;
            found_out = true;
            return;
        }

        depth--;

    }

    /* Leaf search */
    {
        /* Assume cur_node_ptr now pointing to leaf of interest*/
        btree2_leaf_t leaf;
        leaf.leaf_native = vector<uint8_t>((size_t)(curr_node_ptr->node_nrec * hdr->nrec_size));

        openLeafNode(hdr, curr_node_ptr, &leaf, curr_node_ptr->addr);

        /* Locate record */
        locateRecordBTreeV2(hdr, leaf.nrec, hdr->nat_off.data(), leaf.leaf_native.data(), udata, &idx, &cmp);

        if (cmp != 0) {
            *found = false;
        }
        else {
            *found = true;
            found_out = true;
        }

    }

}