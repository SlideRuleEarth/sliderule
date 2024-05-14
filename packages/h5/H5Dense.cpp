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
 * RELEVANT HDF5 SRC LIBRARY
 * https://github.com/HDFGroup/hdf5/blob/45ac12e6b660edfb312110d4e3b4c6970ff0585a/src/H5Adense.c#L322
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
 * Finally, the H5Dense returns control flow to the H5Coro program via the H5BTreeV2 structure
 * On success, the structure provides the address of the Attribute Message.
 *
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5Dense.h"
#include "core.h"
#include <limits>

/******************************************************************************
 * H5 BTREE V2 CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5BTreeV2::H5BTreeV2(uint64_t _fheap_addr, uint64_t name_bt2_addr, const char *_name,  H5FileBuffer::heap_info_t* heap_info_ptr, H5FileBuffer* h5file)
{
    /* Stack */
    const vector<btree2_node_info_t> _node_info;
    const vector<size_t> _nat_off;
    vector<uint64_t> row_block_size;
    vector<uint64_t> row_block_off;
    vector<uint64_t> row_tot_dblock_free;
    vector<uint64_t> row_max_dblock_free;

    /* Init outputs */
    pos_out = 0;
    hdr_flags_out = 0;
    hdr_dlvl_out = 0;
    msg_size_out = 0;
    found_attr = false;

    /* Init H5FileBuff Ptr */
    h5filePtr_ = h5file;

    /* Init UserData (Udata) */
    fheap_addr = _fheap_addr;
    fheap_info = heap_info_ptr;
    name = _name;
    name_hash = checksumLookup3(_name, strlen(_name), 0);

    /* Init portion of BTree Hdr */
    addr = name_bt2_addr;
    max_nrec_size = 0;
    parent = NULL;
    nrec_size = 0;
    node_size = 0;
    rrec_size = 0;
    depth = 0;
    split_percent = 0;
    merge_percent = 0;
    // root = NULL;
    check_sum = 0;
    node_info = _node_info;
    nat_off = _nat_off;

    /* BEGIN DENSE ATTRIBUTE READ */

    /* NOT IMPLEMENTED: SHARED ATTR SUPPORT */
    print2term("WARNING: isTypeSharedAttrs is NOT implemented for dense attr reading \n");
#if 0
    bool shared_attributes = isTypeSharedAttrs(H5FileBuffer::ATTRIBUTE_MSG);
    if(shared_attributes)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "sharedAttribute reading is not implemented");
    }
#endif

    /*** INIT BTREE HDR ***/

    /* Populate header */
    uint64_t pos = addr;
    const uint32_t signature = (uint32_t)h5filePtr_->readField(4, &pos);

    /* Signature check */
    if(signature != H5FileBuffer::H5_V2TREE_SIGNATURE_LE)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid btree header signature: 0x%llX", (unsigned long long)signature);
    }
    /* Version check */
    const uint8_t version = (uint8_t) h5filePtr_->readField(1, &pos);
    if(version != 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid btree header version: %hhu", version);
    }

    /* Record type extraction */
    type = (btree2_subid_t) h5filePtr_->readField(1, &pos);

    /* Cont. parse Btreev2 header */
    node_size = (uint32_t) h5filePtr_->readField(4, &pos);
    rrec_size = (uint16_t) h5filePtr_->readField(2, &pos);
    depth = (uint16_t) h5filePtr_->readField(2, &pos);
    split_percent = (uint8_t) h5filePtr_->readField(1, &pos);
    merge_percent = (uint8_t) h5filePtr_->readField(1, &pos);
    root.addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
    root.node_nrec = (uint16_t)h5filePtr_->readField(2, &pos);
    root.all_nrec = h5filePtr_->readField(h5filePtr_->metaData.lengthsize, &pos);
    check_sum = h5filePtr_->readField(4, &pos);

    /* allocate native record size using type found */
    switch(type) {
        case H5B2_ATTR_DENSE_NAME_ID:
            nrec_size = sizeof(btree2_type8_densename_rec_t);
            break;
        default:
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented type for nrec_size: %d", type);
    }

    if (node_size == 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "0 return in bt2_node_size, failure suspect in openBtreeV2");
    }

    /*** END BTREE HDR ***/

    /*** INIT NODE INFO ***/

    /* Main node info hdr */
    node_info.resize(depth + 1);

    /* Leaf node info */
    sz_max_nrec = (((node_size) - H5B2_METADATA_PREFIX_SIZE) / (rrec_size));

    safeAssigned(node_info[0].max_nrec, sz_max_nrec);
    node_info[0].max_nrec = (uint32_t) sz_max_nrec;
    node_info[0].split_nrec = (node_info[0].max_nrec * split_percent) / 100;
    node_info[0].merge_nrec = (node_info[0].max_nrec * merge_percent) / 100;
    node_info[0].cum_max_nrec = node_info[0].max_nrec;
    node_info[0].cum_max_nrec_size = 0;

    /* alloc array of pointers to internal node native keys */
    nat_off.resize(node_info[0].max_nrec);

    /* Initialize offsets in native key block */
    if (node_info[0].max_nrec != 0 ){
        for (uint32_t u = 0; u < node_info[0].max_nrec; u++) {
            nat_off[u] = nrec_size * u;
        }
    }
    else {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Critical natural offset init failure");
    }

    /* Compute size to compute num records in each record */
    u_max_nrec_size = (log2Gen((uint64_t)node_info[0].max_nrec) / 8) + 1;

    safeAssigned(max_nrec_size, u_max_nrec_size);
    max_nrec_size = (uint8_t) u_max_nrec_size;
    assert(max_nrec_size <= H5B2_SIZEOF_RECORDS_PER_NODE);

    /*** END NODE INFO ***/

    /*** INIT DOUBLE TABLE  ***/

    /* offsets and sizes */
    dtable.start_bits = log2Of2((uint32_t)fheap_info->starting_blk_size);
    dtable.first_row_bits = dtable.start_bits + log2Of2((uint32_t)fheap_info->table_width);
    dtable.max_root_rows = (fheap_info->max_heap_size - dtable.first_row_bits) + 1;
    dtable.max_direct_bits = log2Of2((uint32_t)fheap_info->max_dblk_size);
    dtable.max_direct_rows = (dtable.max_direct_bits - dtable.start_bits) + 2;
    dtable.num_id_first_row = (uint64_t) (fheap_info->starting_blk_size * fheap_info->table_width);
    dtable.max_dir_blk_off_size = sizeOffsetLen(fheap_info->max_dblk_size);
    dtable.curr_root_rows = (uint32_t) fheap_info->curr_num_rows;
    dtable.table_addr = fheap_info->root_blk_addr;

    row_block_size.resize(dtable.max_root_rows);
    row_block_off.resize(dtable.max_root_rows);
    row_tot_dblock_free.resize(dtable.max_root_rows);
    row_max_dblock_free.resize(dtable.max_root_rows);

    dtable.row_block_size = row_block_size;
    dtable.row_block_off = row_block_off;
    dtable.row_tot_dblock_free = row_tot_dblock_free;
    dtable.row_max_dblock_free = row_max_dblock_free;

    /* buid block sizing for each row*/
    uint64_t tmp_block_size = (uint64_t) fheap_info->starting_blk_size;
    uint64_t acc_block_off = (uint64_t) fheap_info->starting_blk_size * fheap_info->table_width;
    dtable.row_block_size[0] = (uint64_t) fheap_info->starting_blk_size;
    dtable.row_block_off[0] = 0;

    for (size_t j = 1; j < dtable.max_root_rows; j++) {
        dtable.row_block_size[j] = tmp_block_size;
        dtable.row_block_off[j] = acc_block_off;
        tmp_block_size *= 2;
        acc_block_off *= 2;
    }

    /*** END DTABLE ***/

    /*** INIT FIND ALGORITHM ***/

    findBTreeV2();

    /*** END FIND ALGORITHM ***/

    /*** ATTRIBUTE STATUS SUCCESS ***/
    if (!found_attr) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "FAILED to locate attribute with dense btreeV2 reading");
    }
    /*** END ***/
}

 /*----------------------------------------------------------------------------
 * isTypeSharedAttrs
 *----------------------------------------------------------------------------*/
bool H5BTreeV2::isTypeSharedAttrs (uint32_t type_id) {
    /* Equiv to H5SM_type_shared of HDF5 SRC Lib: https://github.com/HDFGroup/hdf5/blob/develop/src/H5SM.c#L328 */
    print2term("TODO: isTypeSharedAttrs implementation, omit support for v2 btree, recieved type_id %u \n", type_id);
    return false;
}

/*----------------------------------------------------------------------------
 * log2Gen - helper determines the log base two of a number
 *----------------------------------------------------------------------------*/
uint32_t H5BTreeV2::log2Gen(uint64_t n) {
    /* Taken from https://github.com/HDFGroup/hdf5/blob/develop/src/H5VMprivate.h#L357 */
    uint32_t r; // r will be log2(n)
    uint32_t t, tt, ttt; // temporaries

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

    if ((ttt = (uint32_t)(n >> 32)))
        if ((tt = (uint32_t)(n >> 48)))
            r = (t = (uint32_t)(n >> 56)) ? 56 + (uint32_t)LogTable256[t] : 48 + (uint32_t)LogTable256[tt & 0xFF];
        else
            r = (t = (uint32_t)(n >> 40)) ? 40 + (uint32_t)LogTable256[t] : 32 + (uint32_t)LogTable256[ttt & 0xFF];
    else if ((tt = (uint32_t)(n >> 16)))
        r = (t = (uint32_t)(n >> 24)) ? 24 + (uint32_t)LogTable256[t] : 16 + (uint32_t)LogTable256[tt & 0xFF];
    else
        // added 'uint8_t' cast to pacify PGCC compiler
        r = (t = (uint32_t)(n >> 8)) ? 8 + (uint32_t)LogTable256[t] : (uint32_t)LogTable256[(uint8_t)n];

    return (r);

}

/*----------------------------------------------------------------------------
 * log2of2 - helper replicating H5VM_log2_of2
 *----------------------------------------------------------------------------*/
uint32_t H5BTreeV2::log2Of2(uint32_t n) {
    assert((!(n & (n - 1)) && n));

    static constexpr const uint32_t MultiplyDeBruijnBitPosition[32] = {
        0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
        15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
        16, 7,  26, 12, 18, 6,  11, 5,  10, 9
    };

    return (uint32_t)(MultiplyDeBruijnBitPosition[(n * (uint32_t)0x077CB531UL) >> 27]);
}

/*----------------------------------------------------------------------------
 * sizeOffsetBits
 *----------------------------------------------------------------------------*/
uint16_t H5BTreeV2::sizeOffsetBits(uint16_t b) {
    /* Compute the # of bytes required to store an offset into a given buffer size - taken from h5 macro */
    return (((b) + 7) / 8);
}


/*----------------------------------------------------------------------------
 * sizeOffsetLen
 *----------------------------------------------------------------------------*/
uint16_t H5BTreeV2::sizeOffsetLen(int32_t l) {
    /* Offset Len spinning off bit size - taken from h5 macro */
    return sizeOffsetBits((uint16_t) log2Of2((uint32_t) l));
}

/*----------------------------------------------------------------------------
 * lookup3Rot
 *----------------------------------------------------------------------------*/
uint32_t H5BTreeV2::lookup3Rot(uint32_t x, uint32_t k) {
    return (((x) << (k)) ^ ((x) >> (32 - (k))));

}

/*----------------------------------------------------------------------------
 * lookup3Mix
 *----------------------------------------------------------------------------*/
void H5BTreeV2::lookup3Mix(uint32_t& a,uint32_t& b, uint32_t& c) {
    a -= c;
    a ^= lookup3Rot(c, 4);
    c += b;
    b -= a;
    b ^= lookup3Rot(a, 6);
    a += c;
    c -= b;
    c ^= lookup3Rot(b, 8);
    b += a;
    a -= c;
    a ^= lookup3Rot(c, 16);
    c += b;
    b -= a;
    b ^= lookup3Rot(a, 19);
    a += c;
    c -= b;
    c ^= lookup3Rot(b, 4);
    b += a;
}

/*----------------------------------------------------------------------------
 * lookup3Final
 *----------------------------------------------------------------------------*/
void H5BTreeV2::lookup3Final(uint32_t& a, uint32_t& b, uint32_t& c) {
    c ^= b;
    c -= lookup3Rot(b, 14);
    a ^= c;
    a -= lookup3Rot(c, 11);
    b ^= a;
    b -= lookup3Rot(a, 25);
    c ^= b;
    c -= lookup3Rot(b, 16);
    a ^= c;
    a -= lookup3Rot(c, 4);
    b ^= a;
    b -= lookup3Rot(a, 14);
    c ^= b;
    c -= lookup3Rot(b, 24);

}

/*----------------------------------------------------------------------------
 * checksumLookup3 - helper for jenkins hash
 *----------------------------------------------------------------------------*/

uint32_t H5BTreeV2::checksumLookup3(const void *key, size_t length, uint32_t initval) {
    /* Source: https://github.com/HDFGroup/hdf5/blob/develop/src/H5checksum.c#L365 */

    /* Initialize set up */
    const uint8_t *k = reinterpret_cast<const uint8_t *>(key);
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
        lookup3Mix(a, b, c);
        length -= 12;
        k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length) /* all the case statements fall through */
    {
        case 12:
            c += ((uint32_t)k[11]) << 24;
            // fall through
        case 11:
            c += ((uint32_t)k[10]) << 16;
            // fall through
        case 10:
            c += ((uint32_t)k[9]) << 8;
            // fall through
        case 9:
            c += k[8];
            // fall through
        case 8:
            b += ((uint32_t)k[7]) << 24;
            // fall through
        case 7:
            b += ((uint32_t)k[6]) << 16;
            // fall through
        case 6:
            b += ((uint32_t)k[5]) << 8;
            // fall through
        case 5:
            b += k[4];
            // fall through
        case 4:
            a += ((uint32_t)k[3]) << 24;
            // fall through
        case 3:
            a += ((uint32_t)k[2]) << 16;
            // fall through
        case 2:
            a += ((uint32_t)k[1]) << 8;
            // fall through
        case 1:
            a += k[0];
            break;
        case 0:
            return c;
        default:
            assert(0 && "This should never be executed!");
    }

    lookup3Final(a, b, c);

    return c;

}

/*----------------------------------------------------------------------------
 * safeAssigned - verify that value doesn't overflow when casted to type T
 *----------------------------------------------------------------------------*/
template<typename T, typename V>
void H5BTreeV2::safeAssigned(T& type_verify, V& value) {
    if (value <= std::numeric_limits<T>::max()) {
        type_verify = (T)value;
    }
    else {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Type V value exceeds maximum representation of type T");
    }
}

 /*----------------------------------------------------------------------------
 * addrDecode - helper
 *----------------------------------------------------------------------------*/
void H5BTreeV2::addrDecode(size_t addr_len, const uint8_t **pp, uint64_t* addr_p) {
    /* Decode from buffer at **pp (aka pos), out into addr_p */
    /* Updates the pointer to point to the next byte after the address */

    bool all_zero = true;
    uint32_t u;

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


void H5BTreeV2::varDecode(uint8_t* p, int32_t n, uint8_t l) {
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
    btree2_type5_densename_rec_t *nrecord = reinterpret_cast<btree2_type5_densename_rec_t *>(_nrecord);
    const size_t H5G_DENSE_FHEAP_ID_LEN = 7;
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

    uint32_t u = 0;
    btree2_type8_densename_rec_t *nrecord = reinterpret_cast<btree2_type8_densename_rec_t *>(_nrecord);

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
void H5BTreeV2::fheapLocate(const void * _id) {

    /* Dispatcher for heap ID types - currently only supporting manual type */
    uint8_t* id = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(_id));
    uint8_t id_flags = 0; // heap ID flag bits
    id_flags = *id;

    if ((id_flags & H5HF_ID_VERS_MASK) != H5HF_ID_VERS_CURR) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Incorrect heap ID version");
    }

    /* Check type of object in heap */
    if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_MAN) {
        /* Operate on object from managed heap blocks */
        fheapLocateManaged(id);
    }
    else if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_HUGE) {
        /* NOT IMPLEMENTED - Operate on 'huge' object from file */
        throw RunTimeException(CRITICAL, RTE_ERROR, "Huge heap ID reading not supported");
    }
    else if ((id_flags & H5HF_ID_TYPE_MASK) == H5HF_ID_TYPE_TINY) {
        /* NOT IMPLEMENTED - Operate on 'tiny' object from file */
        throw RunTimeException(CRITICAL, RTE_ERROR, "Tiny heap ID reading not supported");
    }
    else {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unsupported Heap ID");
    }

}

/*----------------------------------------------------------------------------
 * dtableLookup
 *----------------------------------------------------------------------------*/
void H5BTreeV2::dtableLookup(uint64_t off, uint32_t *row, uint32_t *col) {

    uint64_t eval;

    /* Check for offset in first row */
    if (off < dtable.num_id_first_row) {
        *row = 0;
        eval = (off / fheap_info->starting_blk_size);
        safeAssigned(*col, eval);
        *col = (uint32_t) (off / fheap_info->starting_blk_size);
    }
    else {
        const uint32_t high_bit = log2Gen(off); // determine the high bit in the offset
        const uint64_t off_mask = ((uint64_t)1) << high_bit; // compute mask for determining column

        *row = (high_bit - dtable.first_row_bits) + 1;
        eval = ((off - off_mask) / dtable.row_block_size[*row]);
        safeAssigned(*col, eval);
        *col = ((off - off_mask) / dtable.row_block_size[*row]);
    }

}

/*----------------------------------------------------------------------------
 * buildEntriesIndirect
 *----------------------------------------------------------------------------*/
uint64_t H5BTreeV2::buildEntriesIndirect(int32_t nrows, uint64_t pos, uint64_t* ents) {
    /* Build array of addresses (ent) for this indirect block - follow H5HF__cache_iblock_deserialize x readIndirectBlock */
    uint32_t idx = 0;       // track indx in ents
    uint64_t block_off = 0; // iblock->block_off for moving offset

    pos += 5;                               // signature and version
    pos += h5filePtr_->metaData.offsetsize; // skip block header

    /* Build equivalent of iblock->block_off - copy indirect style*/
    const int MAX_BLOCK_OFFSET_SIZE = 8;
    uint8_t block_offset_buf[MAX_BLOCK_OFFSET_SIZE] = {0};
    h5filePtr_->readByteArray(block_offset_buf, fheap_info->blk_offset_size, &pos); // Block Offset
    memcpy(&block_off, block_offset_buf, sizeof(uint64_t));

    for(int32_t row = 0; row < nrows; row++)
    {
        /* Calculate Row's Block Size */
        int32_t row_block_size;
        if (row == 0) {
            row_block_size = fheap_info->starting_blk_size;
        }
        else if (row == 1) {
            row_block_size = fheap_info->starting_blk_size;
        }
        else {
            row_block_size = fheap_info->starting_blk_size * (0x2 << (row - 2));
        }

        /* Process Entries in Row */
        for(int32_t entry = 0; entry < fheap_info->table_width; entry++)
        {
            /* Direct Block Entry */
            if(row_block_size <= fheap_info->max_dblk_size)
            {
                /* Read Direct Block Address and Assign */
                const uint64_t direct_block_addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
                ents[idx] = direct_block_addr;
                idx++;

            }
            else /* Indirect Block Entry and Assign */
            {
                /* Read Indirect Block Address */
                const uint64_t indirect_block_addr = h5filePtr_->readField(h5filePtr_->metaData.offsetsize, &pos);
                ents[idx] = indirect_block_addr;
                idx++;
            }
        }
    }

    return block_off;

}

/*----------------------------------------------------------------------------
 * manualDblockLocate
 *----------------------------------------------------------------------------*/
void H5BTreeV2::manualDblockLocate(uint64_t obj_off, uint64_t* ents, uint32_t *ret_entry) {
    /* Mock implementation of H5HF__man_dblock_locate */
    // iblock->ents <-- ents derived from where H5HF_indirect_t **ret_iblock passed

    uint64_t iblock_addr = 0;
    uint32_t row = 0, col = 0;
    uint64_t block_off = 0;

    /* Look up row & column for object */
    dtableLookup(obj_off, &row, &col);

    /* Set indirect */
    iblock_addr = dtable.table_addr;

    /* Read indirect - set up ents array */
    int32_t nrows = fheap_info->curr_num_rows;
    block_off = buildEntriesIndirect(nrows, iblock_addr, ents);

    /* Check for indirect block row - iterates till direct hit */
    while (row >= dtable.max_direct_rows) {
        uint32_t entry;
        /* Compute # of rows in child indirect block */
        nrows = (log2Gen(dtable.row_block_size[row]) - dtable.first_row_bits) + 1;
        entry = (row * (uint32_t)fheap_info->table_width) + col;
        /* Locate child indirect block */
        iblock_addr = ents[entry];
        /* new iblock search and set - row col - remove offset worht of iblock*/
        dtableLookup((obj_off - block_off), &row, &col);
        /* Switch to new block - new ents list */
        block_off = buildEntriesIndirect(nrows, iblock_addr, ents);

    }

    /* populate entry pointer using derived info */
    *ret_entry = (row * (uint32_t) fheap_info->table_width) + col;
}

/*----------------------------------------------------------------------------
 * fheapLocateManaged
 *----------------------------------------------------------------------------*/
void H5BTreeV2::fheapLocateManaged(uint8_t* id){
    /* Operate on managed heap - eqiv to hdf5 ref functions: H5HF__man_op, H5HF__man_op_real*/

    uint64_t dblock_addr = 0; // found direct block to apply offset on
    uint64_t dblock_block_off = 0; // dblock block offset, extracted from top of dir block header
    uint64_t obj_off = 0; // offset of object in heap
    size_t obj_len = 0; // len of object in heap
    size_t blk_off = 0; // offset of object in block

    id++; // skip due to flag reading
    memcpy(&obj_off, id, (size_t)(fheap_info->heap_off_size));
    id += fheap_info->heap_off_size;
    memcpy(&obj_len, id, (size_t)(fheap_info->heap_len_size));

    /* Locate direct block of interest */
    if(dtable.curr_root_rows == 0)
    {
        /* Direct Block - set address of block before deploying down readDirectBlock */
        dblock_addr = dtable.table_addr;
    }
    else
    {
        /* Indirect Block Navigation */
        const size_t num_elements = static_cast<size_t>(fheap_info->curr_num_rows) * fheap_info->table_width;
        uint64_t* ents = new uint64_t[num_elements](); // mimic H5HF_indirect_ent_t of the H5HF_indirect_t struct
        uint32_t entry; // entry of block

        /* Search for direct block using double table and offset */
        manualDblockLocate(obj_off, ents, &entry);
        dblock_addr = ents[entry];

        /* Free allocated memory */
        delete[] ents;
    }

    /* read direct block to access message */
    uint64_t pos = dblock_addr;
    pos += 5; // skip signature and version of object
    pos += h5filePtr_->metaData.offsetsize; // skip heap hdr addr

    /* Unpack block offset */
    const int MAX_BLOCK_OFFSET_SIZE = 8;
    uint8_t new_block_offset_buf[MAX_BLOCK_OFFSET_SIZE] = {0};
    h5filePtr_->readByteArray(new_block_offset_buf, fheap_info->blk_offset_size, &pos);
    memcpy(&dblock_block_off, new_block_offset_buf, sizeof(uint64_t));

    /* Checksum */
    // TODO (only present if flags 1 see spec)

    /* position pointer inside of dblock to read object */
    blk_off = (size_t)(obj_off - dblock_block_off);
    pos = dblock_addr + (uint64_t) blk_off;
    const uint64_t msg_size = (uint64_t) obj_len;

    /* read object switch on mssg type */
    switch(type) {
        case H5B2_ATTR_DENSE_NAME_ID:
            /* return info for outside readAttributeMessage - complete obj construction*/
            pos_out = pos;
            hdr_flags_out = fheap_info->hdr_flags;
            hdr_dlvl_out = fheap_info->dlvl;
            msg_size_out = msg_size;
            break;
        default:
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented hdr->type message read: %d", type);
    }

}

/*----------------------------------------------------------------------------
 * fheapNameCmp
 *----------------------------------------------------------------------------*/
void H5BTreeV2::fheapNameCmp(const void *obj, size_t obj_len, const void *op_data){

    // temp satisfy print
    print2term("fheapNameCmp args: %lu, %lu, %lu",
                static_cast<unsigned long>(reinterpret_cast<uintptr_t>(obj)),
                static_cast<unsigned long>(obj_len),
                static_cast<unsigned long>(reinterpret_cast<uintptr_t>(op_data)));
    // TODO
}

 /*----------------------------------------------------------------------------
 * compareType8Record
 *----------------------------------------------------------------------------*/
void H5BTreeV2::compareType8Record(const void *_bt2_rec, int32_t *result)
{
    /* Implementation of H5A__dense_btree2_name_compare with type 8 - H5B2_GRP_DENSE_NAME_ID*/
    /* See: https://github.com/HDFGroup/hdf5/blob/0ee99a66560422fc20864236a83bdcd0103d8f64/src/H5Abtree2.c#L220 */

    const btree2_type8_densename_rec_t *bt2_rec = static_cast<const btree2_type8_densename_rec_t *>(_bt2_rec);

    /* Check hash value - influence btree search direction */
    if (name_hash < bt2_rec->hash)
        *result = (-1);
    else if (name_hash > bt2_rec->hash)
        *result = 1;
    else {
        /* Hash match identified! */

        /* Sanity check */
        assert(name_hash == bt2_rec->hash);

        /* Check if shared fractal heap - NOT IMPLEMENTED */
        if (bt2_rec->flags & H5O_MSG_FLAG_SHARED) {
            throw RunTimeException(CRITICAL, RTE_ERROR, "No support implemented for shared fractal heaps");
        }

        /* Locate object in fractal heap */
        fheapLocate(&bt2_rec->id);
        *result = 0;

    }
}

 /*----------------------------------------------------------------------------
 * locateRecordBTreeV2
 *----------------------------------------------------------------------------*/
void H5BTreeV2::locateRecordBTreeV2(uint32_t nrec, const size_t *rec_off, const uint8_t *native, uint32_t *idx, int32_t *cmp) {
    /* Performs a binary search to locate a record in a sorted array of records
    sets *idx to location of record greater than or equal to record to locate */
    /* hdf5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2int.c#L89 */

    uint32_t lo = 0, hi = nrec; // bounds of search range
    uint32_t my_idx = 0; // current record idx

    *cmp = -1;

    while (lo < hi && *cmp) {
        /* call on type compare method */
        my_idx = (lo + hi) / 2;
        switch(type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                compareType8Record(native + rec_off[my_idx], cmp);
                break;
            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented type compare: %d", type);
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
 * openInternalNode
 *----------------------------------------------------------------------------*/
void H5BTreeV2::openInternalNode(btree2_internal_t *internal, uint64_t internal_pos, const btree2_node_ptr_t* curr_node_ptr) {
    /* Set up internal node structure from given addr start: internal_pos */

    uint8_t *native = NULL;
    uint32_t u = 0;
    int32_t node_nrec = 0;

    /* Signature sanity check */
    const uint32_t signature = (uint32_t) h5filePtr_->readField(4, &internal_pos);
    if (signature != H5FileBuffer::H5_V2TREE_INTERNAL_SIGNATURE_LE) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Signature does not match internal node: %u", signature);
    }

    /* Version check */
    const uint8_t version = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if (version != 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid version for internal node: %u", version);
    }

    /* B-tree Type check */
    const uint8_t _type = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if ((btree2_subid_t)_type != type) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid type for internal node: %u, expected from hdr: %u", _type, type);
    }

    /* Data Intake Prep -  H5B2__cache_int_deserialize */
    internal->nrec = curr_node_ptr->node_nrec; // assume internal reflected in current ptr
    internal->depth = depth;
    internal->parent = parent;

    /* Deserialize records*/
    native = internal->int_native.data();
    for (u = 0; u < internal->nrec; u++) {

        /* Decode record via set decode method from type- modifies native arr directly */
        switch(type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                internal_pos = decodeType8Record(internal_pos, native);
                break;
            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented type for decode: %d", type);
        }

        /* Move to next record */
        native += nrec_size;

    }

    /* Deserialize node pointers for internal node */
    btree2_node_ptr_t *int_node_ptr = internal->node_ptrs.data();
    for (u = 0; u < (uint32_t)(internal->nrec + 1); u++) {
        /* Decode node address -- see H5F_addr_decode */
        const size_t addr_size = (size_t) h5filePtr_->metaData.offsetsize; // as defined by hdf spec

        addrDecode(addr_size, reinterpret_cast<const uint8_t **>(&internal_pos), &(int_node_ptr->addr)); // internal pos value should change
        varDecode(reinterpret_cast<uint8_t *>(internal_pos), node_nrec, max_nrec_size); // NOLINT(performance-no-int-to-ptr)

        safeAssigned(int_node_ptr->node_nrec, node_nrec);
        int_node_ptr->node_nrec = (uint16_t) node_nrec;

        if (internal->depth > 1) {
            varDecode(reinterpret_cast<uint8_t *>(internal_pos), int_node_ptr->all_nrec, node_info[internal->depth - 1].cum_max_nrec_size); // NOLINT(performance-no-int-to-ptr)
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
}

/*----------------------------------------------------------------------------
 * openLeafNode
 *----------------------------------------------------------------------------*/
uint64_t H5BTreeV2::openLeafNode(const btree2_node_ptr_t *curr_node_ptr, btree2_leaf_t *leaf, uint64_t internal_pos) {
    /* given pointer to lead node, set *leaf struct and deserialize the records contained at the node */
    /* hdf5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2cache.c#L988 */

    uint8_t *native = NULL;
    uint32_t u = 0;

    /* Signature Check*/
    const uint32_t signature = (uint32_t) h5filePtr_->readField(4, &internal_pos);
    if (signature != H5FileBuffer::H5_V2TREE_LEAF_SIGNATURE_LE) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Signature does not match leaf node: %u", signature);
    }

    /* Version check */
    const uint8_t version = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if (version != 0) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Version does not match leaf node: %u", version);
    }

    /* Type check */
    const uint8_t _type = (uint8_t) h5filePtr_->readField(1, &internal_pos);
    if ((btree2_subid_t) _type != type) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Type of leaf node: %u, does not match header type: %u", _type, type);
    }

    /* Allocate space for the native keys in memory & set num records */
    leaf->nrec = curr_node_ptr->node_nrec;

    /* Deserialize records*/
    native = leaf->leaf_native.data();
    for (u = 0; u < leaf->nrec; u++) {
        /* Type switching */
        switch(type) {
            case H5B2_ATTR_DENSE_NAME_ID:
                internal_pos = decodeType8Record(internal_pos, native);
                break;

            default:
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unimplemented type for decode: %d", type);
        }
        /* move to populate next record*/
        native += nrec_size;
    }

    /* Checksum */
    // TODO

    /* Return final movement */
    return internal_pos;

}

 /*----------------------------------------------------------------------------
 * findBTreeV2
 *----------------------------------------------------------------------------*/
 void H5BTreeV2::findBTreeV2 () {
    /* Given start of btreev2, search for matching udata (priv vars) */
    /* HDF5 ref implementation: https://github.com/HDFGroup/hdf5/blob/cc50a78000a7dc536ecff0f62b7206708987bc7d/src/H5B2.c#L429 */

    btree2_node_ptr_t* curr_node_ptr = NULL; // node ptr info for curr
    uint16_t _depth = 0;                     // of tree
    int32_t cmp = 0;                             // comparison value of records (0 if found, else -1 or 1 for search direction)
    uint32_t idx = 0;                        // location (index) of record which matches key
    btree2_nodepos_t curr_pos;               // address of curr node

    /* Copy root ptr - exit if empty tree */
    curr_node_ptr = &root;

    /* Sanity check: no records at node imply finished search */
    if (curr_node_ptr->node_nrec == 0) {
        found_attr = false;
        return;
    }

    /* Skip min/max accelerated search; note "SWMR writes" - TODO in future*/
    _depth = depth;

    /* Walk down B-tree to find record or leaf node where record is located */
    cmp = -1;
    curr_pos = H5B2_POS_ROOT;

    /* Init internal */
    btree2_node_ptr_t next_node_ptr;
    btree2_internal_t internal;
    internal.int_native = vector<uint8_t>((size_t)(rrec_size * curr_node_ptr->node_nrec));
    internal.node_ptrs = vector<btree2_node_ptr_t>((uint32_t)(curr_node_ptr->node_nrec + 1));

    if (_depth > 0) {
        for (uint32_t u = 1; u < (uint32_t)(_depth + 1); u++) {
            print2term("WARNING: UNTESTED IMPLEMENTATION FOR INTERNAL NODE \n");

            const uint32_t b2_int_ptr_size = (uint32_t)(h5filePtr_->metaData.offsetsize) + max_nrec_size + (node_info[(u)-1]).cum_max_nrec_size; // = H5B2_INT_POINTER_SIZE(h, u)
            sz_max_nrec = ((node_size - (H5B2_METADATA_PREFIX_SIZE + b2_int_ptr_size)) / (rrec_size + b2_int_ptr_size)); // = H5B2_NUM_INT_REC(hdr, u);
            safeAssigned(node_info[u].max_nrec, sz_max_nrec);
            node_info[u].max_nrec = sz_max_nrec;

            assert(node_info[u].max_nrec <= node_info[u - 1].max_nrec);
            node_info[u].split_nrec = (node_info[u].max_nrec * split_percent) / 100;
            node_info[u].merge_nrec = (node_info[u].max_nrec * merge_percent) / 100;
            node_info[u].cum_max_nrec = ((node_info[u].max_nrec + 1) * node_info[u - 1].cum_max_nrec) + node_info[u].max_nrec;
            u_max_nrec_size = (log2Gen(node_info[u].cum_max_nrec) / 8) + 1;

            safeAssigned(node_info[u].cum_max_nrec_size, u_max_nrec_size);
            node_info[u].cum_max_nrec_size= (uint8_t) u_max_nrec_size;

        }

    }

    while (_depth > 0) {

        print2term("WARNING: UNTESTED IMPLEMENTATION FOR INTERNAL NODE \n");

        /* INTERNAL NODE SET UP - Write into internal */
        const uint64_t internal_pos = curr_node_ptr->addr; // snapshot internal addr start
        openInternalNode(&internal, internal_pos, curr_node_ptr); // internal set with all info for locate record

        /* LOCATE RECORD - via type compare method */
        locateRecordBTreeV2(internal.nrec, nat_off.data(), internal.int_native.data(), &idx, &cmp);

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
            found_attr = true;
            return;
        }

        _depth--;

    }

    /* Leaf search */
    {
        /* Assume cur_node_ptr now pointing to leaf of interest*/
        btree2_leaf_t leaf;
        leaf.leaf_native = vector<uint8_t>((size_t)(curr_node_ptr->node_nrec * nrec_size));

        openLeafNode(curr_node_ptr, &leaf, curr_node_ptr->addr);

        /* Locate record */
        locateRecordBTreeV2(leaf.nrec, nat_off.data(), leaf.leaf_native.data(), &idx, &cmp);

        if (cmp != 0) {
            found_attr = false;
        }
        else {
            found_attr = true;
        }

    }

}