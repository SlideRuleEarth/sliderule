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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#ifdef __aws__
#include "S3Lib.h"
#endif

#include "H5Lite.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define H5_INVALID(var)  (var == (0xFFFFFFFFFFFFFFFFllu >> (64 - (sizeof(var) * 8))))

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * get_field
 *----------------------------------------------------------------------------*/
uint64_t get_field(uint8_t* buffer, int buffer_size, int* field_offset, int field_size)
{
    if(*field_offset + field_size >= buffer_size)
    {
        return 0;
    }

    switch(field_size)
    {
        case 8:     
        {
            uint64_t val = *(uint64_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapll(val);
            #else
                return val; 
            #endif
        }

        case 4:     
        {
            uint32_t val = *(uint32_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapl(val);
            #else
                return val; 
            #endif
        }

        case 2:
        {
            uint16_t val = *(uint16_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swaps(val);
            #else
                return val; 
            #endif
        }

        default:
        {
            return 0;
        }
    }
}

/******************************************************************************
 * H5 FILE BUFFER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::H5FileBuffer (const char* filename, const char* _dataset, bool _errorChecking, bool _verbose)
{
    assert(filename);
    assert(_dataset);

    errorChecking = _errorChecking;
    verbose = _verbose;

    /* Initialize */
    buffSize = 0;
    currFilePosition = 0;
    offsetSize = 0;
    lengthSize = 0;
    groupLeafNodeK = 0;
    groupInternalNodeK = 0;
    rootGroupOffset = 0;
    
    /* Initialize Data */
    dataType = UNKNOWN_TYPE;
    dataElementSize = 0;
    dataFill.fill_ll = 0LL;
    dataSize = 0;
    dataBuffer = NULL;
    dataDimensions = NULL;
    dataNumDimensions = 0;
    dataFilter = INVALID_FILTER;
    dataFilterParms = NULL;
    dataNumFilterParms = 0;

    /* Open File */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        mlog(CRITICAL, "Failed to open filename: %s", filename);
        throw std::runtime_error("failed to open file");
    }

    /* Get Dataset Path */
    parseDataset(_dataset); // populates class members

    /* Read Superblock */
    readSuperblock(); // populates class members

    /* Start at Root Group */
    readObjHdr(rootGroupOffset, 0);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::~H5FileBuffer (void)
{
    fclose(fp);
    if(dataBuffer) delete [] dataBuffer;
    if(dataDimensions) delete [] dataDimensions;
    if(dataFilterParms) delete [] dataFilterParms;
}

/*----------------------------------------------------------------------------
 * getCurrPos
 *----------------------------------------------------------------------------*/
void H5FileBuffer::parseDataset (const char* _dataset)
{
    /* Create Copy of Dataset */
    dataset = StringLib::duplicate(_dataset);

    /* Get Pointer to First Group in Dataset */
    const char* gptr; // group pointer
    if(dataset[0] == '/')   gptr = &dataset[1];
    else                    gptr = &dataset[0];

    /* Build Path to Dataset */
    while(true)
    {        
        datasetPath.add(gptr);                      // add group to dataset path
        char* nptr = StringLib::find(gptr, '/');    // look for next group marker
        if(nptr == NULL) break;                     // if not found, then exit
        *nptr = '\0';                               // terminate group string
        gptr = nptr + 1;                            // go to start of next group
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Dataset: ");
        for(int g = 0; g < datasetPath.length(); g++)
        {
            mlog(RAW, "/%s", datasetPath[g]);
        }
        mlog(RAW, "\n----------------\n");
    }
}

/*----------------------------------------------------------------------------
 * type2str
 *----------------------------------------------------------------------------*/
const char* H5FileBuffer::type2str (data_type_t datatype)
{
    switch(datatype)
    {
        case FIXED_POINT_TYPE:      return "FIXED_POINT_TYPE";
        case FLOATING_POINT_TYPE:   return "FLOATING_POINT_TYPE";
        case TIME_TYPE:             return "TIME_TYPE";
        case STRING_TYPE:           return "STRING_TYPE";
        case BIT_FIELD_TYPE:        return "BIT_FIELD_TYPE";
        case OPAQUE_TYPE:           return "OPAQUE_TYPE";
        case COMPOUND_TYPE:         return "COMPOUND_TYPE";
        case REFERENCE_TYPE:        return "REFERENCE_TYPE";
        case ENUMERATED_TYPE:       return "ENUMERATED_TYPE";
        case VARIABLE_LENGTH_TYPE:  return "VARIABLE_LENGTH_TYPE";
        case ARRAY_TYPE:            return "ARRAY_TYPE";
        default:                    return "UNKNOWN_TYPE";
    }
}

/*----------------------------------------------------------------------------
 * layout2str
 *----------------------------------------------------------------------------*/
const char* H5FileBuffer::layout2str (layout_t layout)
{
    switch(layout)
    {
        case COMPACT_LAYOUT:    return "COMPACT_LAYOUT";
        case CONTIGUOUS_LAYOUT: return "CONTIGUOUS_LAYOUT";
        case CHUNKED_LAYOUT:    return "CHUNKED_LAYOUT";
        default:                return "UNKNOWN_LAYOUT";
    }
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readField (int size, uint64_t* pos)
{
    assert(pos);
    assert(size > 0);

    int field_size = size;
    uint64_t field_position = *pos;

    /* Check if Different Data Needs to be Buffered */
    if((field_position < currFilePosition) || ((field_position + field_size) > (currFilePosition + buffSize)))
    {
        if(fseek(fp, field_position, SEEK_SET) == 0)
        {
            buffSize = fread(buffer, 1, READ_BUFSIZE, fp);
            currFilePosition = field_position;
        }
        else
        {
            throw std::runtime_error("failed to go to field position");
        }
    }

    /* Set Buffer Position */
    uint64_t buff_offset = field_position - currFilePosition;

    /*  Read Field Value */
    uint64_t value;
    switch(field_size)
    {
        case 8:     
        {
            value = *(uint64_t*)&buffer[buff_offset];
            #ifdef __BE__
                value = LocalLib::swapll(value);
            #endif
            break;
        }

        case 4:     
        {
            value = *(uint32_t*)&buffer[buff_offset];
            #ifdef __BE__
                value = LocalLib::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *(uint16_t*)&buffer[buff_offset];
            #ifdef __BE__
                value = LocalLib::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = buffer[buff_offset];
            break;
        }

        default:
        {
            assert(0);
            throw std::runtime_error("invalid field size");
        }
    }

    /* Increment Position */
    *pos += field_size;

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * readData
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readData (uint8_t* data, uint64_t size, uint64_t* pos)
{
    assert(data);
    assert(size > 0);
    assert(pos);

    /* Set Data Position */
    if(fseek(fp, *pos, SEEK_SET) == 0)
    {
        currFilePosition = *pos;
    }
    else
    {
        throw std::runtime_error("failed to go to data position");
    }

    /* Read Data */
    int data_left_to_read = size;
    while(data_left_to_read > 0)
    {
        int data_already_read = size - data_left_to_read;
        int data_to_read = MIN(READ_BUFSIZE, data_left_to_read);
        buffSize = fread(&data[data_already_read], 1, data_to_read, fp);
        data_left_to_read -= buffSize;
        *pos += buffSize;
    }
}

/*----------------------------------------------------------------------------
 * readSuperblock
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readSuperblock (void)
{
    uint64_t pos = 0;

    /* Read and Verify Superblock Info */
    if(errorChecking)
    {
        uint64_t signature = readField(8, &pos);
        if(signature != H5_SIGNATURE_LE)
        {
            mlog(CRITICAL, "Invalid h5 file signature: 0x%llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid signature");
        }

        uint64_t superblock_version = readField(1, &pos);
        if(superblock_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file superblock version: %d\n", (int)superblock_version);
            throw std::runtime_error("invalid superblock version");
        }

        uint64_t freespace_version = readField(1, &pos);
        if(freespace_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file free space version: %d\n", (int)freespace_version);
            throw std::runtime_error("invalid free space version");
        }

        uint64_t roottable_version = readField(1, &pos);
        if(roottable_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file root table version: %d\n", (int)roottable_version);
            throw std::runtime_error("invalid root table version");
        }

        uint64_t headermsg_version = readField(1, &pos);
        if(headermsg_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file header message version: %d\n", (int)headermsg_version);
            throw std::runtime_error("invalid header message version");
        }
    }

    /* Read Sizes */
    pos = 13;
    offsetSize          = readField(1, &pos);
    lengthSize          = readField(1, &pos);
    groupLeafNodeK      = readField(2, &pos);
    groupInternalNodeK  = readField(2, &pos);

    /* Read Group Offset */
    pos = 64;
    rootGroupOffset     = readField(offsetSize, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "File Information\n");
        mlog(RAW, "----------------\n");
        mlog(RAW, "Size of Offsets:                                                 %lu\n",     (unsigned long)offsetSize);
        mlog(RAW, "Size of Lengths:                                                 %lu\n",     (unsigned long)lengthSize);
        mlog(RAW, "Group Leaf Node K:                                               %lu\n",     (unsigned long)groupLeafNodeK);
        mlog(RAW, "Group Internal Node K:                                           %lu\n",     (unsigned long)groupInternalNodeK);
        mlog(RAW, "Root Object Header Address:                                      0x%lX\n",   (long unsigned)rootGroupOffset);
    }

    /* Return Bytes Read */
    return pos;
}

/*----------------------------------------------------------------------------
 * readFractalHeap
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFractalHeap (msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int FRHP_CHECKSUM_DIRECT_BLOCKS = 0x02;

    uint64_t starting_position = pos;

    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FRHP_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid heap signature: 0x%llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid heap signature");
        }

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            mlog(CRITICAL, "invalid heap version: %d\n", (int)version);
            throw std::runtime_error("invalid heap version");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Fractal Heap [%d]: %d, 0x%lx\n", dlvl, (int)type, starting_position);
        mlog(RAW, "----------------\n");
    }

    /*  Read Fractal Heap Header */
    uint16_t    heap_obj_id_len     = (uint16_t)readField(2, &pos); // Heap ID Length
    uint16_t    io_filter_len       = (uint16_t)readField(2, &pos); // I/O Filters' Encoded Length
    uint8_t     flags               =  (uint8_t)readField(1, &pos); // Flags
    uint32_t    max_size_mg_obj     = (uint32_t)readField(4, &pos); // Maximum Size of Managed Objects
    uint64_t    next_huge_obj_id    = (uint64_t)readField(lengthSize, &pos); // Next Huge Object ID
    uint64_t    btree_addr_huge_obj = (uint64_t)readField(offsetSize, &pos); // v2 B-tree Address of Huge Objects
    uint64_t    free_space_mg_blks  = (uint64_t)readField(lengthSize, &pos); // Amount of Free Space in Managed Blocks
    uint64_t    addr_free_space_mg  = (uint64_t)readField(offsetSize, &pos); // Address of Managed Block Free Space Manager
    uint64_t    mg_space            = (uint64_t)readField(lengthSize, &pos); // Amount of Manged Space in Heap
    uint64_t    alloc_mg_space      = (uint64_t)readField(lengthSize, &pos); // Amount of Allocated Managed Space in Heap
    uint64_t    dblk_alloc_iter     = (uint64_t)readField(lengthSize, &pos); // Offset of Direct Block Allocation Iterator in Managed Space
    uint64_t    mg_objs             = (uint64_t)readField(lengthSize, &pos); // Number of Managed Objects in Heap
    uint64_t    huge_obj_size       = (uint64_t)readField(lengthSize, &pos); // Size of Huge Objects in Heap
    uint64_t    huge_objs           = (uint64_t)readField(lengthSize, &pos); // Number of Huge Objects in Heap
    uint64_t    tiny_obj_size       = (uint64_t)readField(lengthSize, &pos); // Size of Tiny Objects in Heap
    uint64_t    tiny_objs           = (uint64_t)readField(lengthSize, &pos); // Number of Timing Objects in Heap
    uint16_t    table_width         = (uint16_t)readField(2, &pos); // Table Width
    uint64_t    starting_blk_size   = (uint64_t)readField(lengthSize, &pos); // Starting Block Size
    uint64_t    max_dblk_size       = (uint64_t)readField(lengthSize, &pos); // Maximum Direct Block Size
    uint16_t    max_heap_size       = (uint16_t)readField(2, &pos); // Maximum Heap Size
    uint16_t    start_num_rows      = (uint16_t)readField(2, &pos); // Starting # of Rows in Root Indirect Block
    uint64_t    root_blk_addr       = (uint64_t)readField(offsetSize, &pos); // Address of Root Block
    uint16_t    curr_num_rows       = (uint16_t)readField(2, &pos); // Current # of Rows in Root Indirect Block

    /* Read Filter Information */
    if(io_filter_len > 0)
    {
        uint64_t filter_root_dblk   = (uint64_t)readField(lengthSize, &pos); // Size of Filtered Root Direct Block
        uint32_t filter_mask        = (uint32_t)readField(4, &pos); // I/O Filter Mask
        mlog(RAW, "Size of Filtered Root Direct Block:                              %lu\n", (unsigned long)filter_root_dblk);
        mlog(RAW, "I/O Filter Mask:                                                 %lu\n", (unsigned long)filter_mask);

        readMessage(FILTER_MSG, io_filter_len, pos, hdr_flags, dlvl);
    }

    if(verbose)
    {
        mlog(RAW, "Heap ID Length:                                                  %lu\n", (unsigned long)heap_obj_id_len);
        mlog(RAW, "I/O Filters' Encoded Length:                                     %lu\n", (unsigned long)io_filter_len);
        mlog(RAW, "Flags:                                                           0x%lx\n", (unsigned long)flags);
        mlog(RAW, "Maximum Size of Managed Objects:                                 %lu\n", (unsigned long)max_size_mg_obj);
        mlog(RAW, "Next Huge Object ID:                                             %lu\n", (unsigned long)next_huge_obj_id);
        mlog(RAW, "v2 B-tree Address of Huge Objects:                               0x%lx\n", (unsigned long)btree_addr_huge_obj);
        mlog(RAW, "Amount of Free Space in Managed Blocks:                          %lu\n", (unsigned long)free_space_mg_blks);
        mlog(RAW, "Address of Managed Block Free Space Manager:                     0x%lx\n", (unsigned long)addr_free_space_mg);
        mlog(RAW, "Amount of Manged Space in Heap:                                  %lu\n", (unsigned long)mg_space);
        mlog(RAW, "Amount of Allocated Managed Space in Heap:                       %lu\n", (unsigned long)alloc_mg_space);
        mlog(RAW, "Offset of Direct Block Allocation Iterator in Managed Space:     %lu\n", (unsigned long)dblk_alloc_iter);
        mlog(RAW, "Number of Managed Objects in Heap:                               %lu\n", (unsigned long)mg_objs);
        mlog(RAW, "Size of Huge Objects in Heap:                                    %lu\n", (unsigned long)huge_obj_size);
        mlog(RAW, "Number of Huge Objects in Heap:                                  %lu\n", (unsigned long)huge_objs);
        mlog(RAW, "Size of Tiny Objects in Heap:                                    %lu\n", (unsigned long)tiny_obj_size);
        mlog(RAW, "Number of Timing Objects in Heap:                                %lu\n", (unsigned long)tiny_objs);
        mlog(RAW, "Table Width:                                                     %lu\n", (unsigned long)table_width);
        mlog(RAW, "Starting Block Size:                                             %lu\n", (unsigned long)starting_blk_size);
        mlog(RAW, "Maximum Direct Block Size:                                       %lu\n", (unsigned long)max_dblk_size);
        mlog(RAW, "Maximum Heap Size:                                               %lu\n", (unsigned long)max_heap_size);
        mlog(RAW, "Starting # of Rows in Root Indirect Block:                       %lu\n", (unsigned long)start_num_rows);
        mlog(RAW, "Address of Root Block:                                           0x%lx\n", (unsigned long)root_blk_addr);
        mlog(RAW, "Current # of Rows in Root Indirect Block:                        %lu\n", (unsigned long)curr_num_rows);
    }

    /* Check Checksum */
    uint64_t check_sum = readField(4, &pos);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Process Direct Block */
    if(curr_num_rows == 0)
    {
        int blk_offset_sz = (max_heap_size + 7) / 8;
        bool checksum_present = (flags & FRHP_CHECKSUM_DIRECT_BLOCKS) != 0;
        int blk_size = starting_blk_size;
        int bytes_read = readDirectBlock(blk_offset_sz, checksum_present, blk_size, mg_objs, type, root_blk_addr, hdr_flags, dlvl);
        if(errorChecking && (bytes_read > blk_size))
        {
            mlog(CRITICAL, "Direct block contianed more bytes than specified: %d > %d\n", bytes_read, blk_size);
            throw std::runtime_error("invalid direct block");            
        }
        pos += blk_size;        
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDirectBlock
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDirectBlock (int blk_offset_size, bool checksum_present, int blk_size, int msgs_in_blk, msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    uint64_t starting_position = pos;

    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FHDB_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid direct block signature: 0x%llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid direct block signature");
        }

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            mlog(CRITICAL, "invalid direct block version: %d\n", (int)version);
            throw std::runtime_error("invalid direct block version");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Direct Block [%d,%d]: 0x%lx\n", dlvl, (int)type, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }
    
    /* Read Block Header */
    if(!verbose)
    {
        pos += offsetSize + blk_offset_size;
    }
    else
    {
        uint64_t heap_hdr_addr = (uint64_t)readField(offsetSize, &pos); // Heap Header Address
        uint64_t blk_offset    = (uint64_t)readField(blk_offset_size, &pos); // Block Offset
        mlog(RAW, "Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        mlog(RAW, "Block Offset:                                                    0x%lx\n", blk_offset);
    }

    if(checksum_present)
    {
        uint64_t check_sum = readField(4, &pos);
        if(errorChecking)
        {
            (void)check_sum;
        }
    }

    /* Read Block Data */
    int data_left = blk_size - (5 + offsetSize + blk_offset_size + ((int)checksum_present * 4));
    for(int i = 0; i < msgs_in_blk && data_left > 0; i++)
    {
        pos += readMessage(type, data_left, pos, hdr_flags, dlvl);
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdr
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readObjHdr (uint64_t pos, int dlvl)
{
    static const int SIZE_OF_CHUNK_0_MASK      = 0x03;
    static const int STORE_CHANGE_PHASE_BIT    = 0x10;
    static const int FILE_STATS_BIT            = 0x20;

    uint64_t starting_position = pos;

    /* Peek at Version / Process Version 1 */
    uint64_t peeking_position = pos;
    uint8_t peek = readField(1, &peeking_position);
    if(peek == 1) return readObjHdrV1(starting_position, dlvl);

    /* Read Object Header */
    if(!errorChecking)
    {
        pos += 5; // move past signature and version
    }
    else
    {
        uint64_t signature = readField(4, &pos);
        if(signature != H5_OHDR_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid header signature: 0x%llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid header signature");
        }

        uint64_t version = readField(1, &pos);
        if(version != 2)
        {
            mlog(CRITICAL, "invalid header version: %d\n", (int)version);
            throw std::runtime_error("invalid header version");
        }
    }

    /* Read Option Time Fields */
    uint8_t obj_hdr_flags = (uint8_t)readField(1, &pos);
    if(obj_hdr_flags & FILE_STATS_BIT)
    {        
        if(!verbose)
        {
            pos += 16; // move past time fields
        }
        else
        {
            uint64_t access_time         = readField(4, &pos);
            uint64_t modification_time   = readField(4, &pos);
            uint64_t change_time         = readField(4, &pos);
            uint64_t birth_time          = readField(4, &pos);

            mlog(RAW, "\n----------------\n");
            mlog(RAW, "Object Information [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
            mlog(RAW, "----------------\n");

            TimeLib::gmt_time_t access_gmt = TimeLib::gettime(access_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Access Time:                                                     %d:%d:%d:%d:%d\n", access_gmt.year, access_gmt.day, access_gmt.hour, access_gmt.minute, access_gmt.second);

            TimeLib::gmt_time_t modification_gmt = TimeLib::gettime(modification_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Modification Time:                                               %d:%d:%d:%d:%d\n", modification_gmt.year, modification_gmt.day, modification_gmt.hour, modification_gmt.minute, modification_gmt.second);

            TimeLib::gmt_time_t change_gmt = TimeLib::gettime(change_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Change Time:                                                     %d:%d:%d:%d:%d\n", change_gmt.year, change_gmt.day, change_gmt.hour, change_gmt.minute, change_gmt.second);

            TimeLib::gmt_time_t birth_gmt = TimeLib::gettime(birth_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Birth Time:                                                      %d:%d:%d:%d:%d\n", birth_gmt.year, birth_gmt.day, birth_gmt.hour, birth_gmt.minute, birth_gmt.second);
        }
    }

    /* Optional Phase Attributes */
    if(obj_hdr_flags & STORE_CHANGE_PHASE_BIT)
    {
        if(!verbose)
        {
            pos += 4; // move past phase attributes
        }
        else
        {
            uint64_t max_compact_attr = readField(2, &pos); (void)max_compact_attr;
            uint64_t max_dense_attr = readField(2, &pos); (void)max_dense_attr;
        }
    }

    /* Read Header Messages */
    uint64_t size_of_chunk0 = readField(1 << (obj_hdr_flags & SIZE_OF_CHUNK_0_MASK), &pos);
    uint64_t end_of_hdr = pos + size_of_chunk0;
    pos += readMessages (pos, end_of_hdr, obj_hdr_flags, dlvl);    

    /* Verify Checksum */
    uint64_t check_sum = readField(4, &pos);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessages
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessages (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    static const int ATTR_CREATION_TRACK_BIT   = 0x04;

    uint64_t starting_position = pos;

    while(pos < end) 
    {
        /* Read Message Info */
        uint8_t     msg_type    = (uint8_t)readField(1, &pos);
        uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;

        if(hdr_flags & ATTR_CREATION_TRACK_BIT)
        {
            uint64_t msg_order = (uint8_t)readField(2, &pos); (void)msg_order;
        }

        /* Read Each Message */
        int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);
        if(errorChecking && (bytes_read != msg_size))
        {
            mlog(CRITICAL, "Header continuation message different size than specified: %d != %d\n", bytes_read, msg_size);
            throw std::runtime_error("invalid header continuation message");            
        }

        /* Update Position */
        pos += bytes_read;
    }

    /* Check Size */
    if(errorChecking)
    {
        if(pos != end)
        {
            mlog(CRITICAL, "Did not read correct number of bytes: %lu != %lu\n", (unsigned long)pos, (unsigned long)end);
            throw std::runtime_error("did not read correct number bytes");            
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdrV1
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readObjHdrV1 (uint64_t pos, int dlvl)
{
    uint64_t starting_position = pos;

    /* Read Version */
    if(!errorChecking)
    {
        pos += 2;
    }
    else
    {
        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 1)
        {
            mlog(CRITICAL, "invalid header version: %d\n", (int)version);
            throw std::runtime_error("invalid header version");
        }

        uint8_t reserved0 = (uint8_t)readField(1, &pos); 
        if(reserved0 != 0)
        {
            mlog(CRITICAL, "invalid reserved field: %d\n", (int)reserved0);
            throw std::runtime_error("invalid reserved field");
        }
    }

    /* Read Number of Header Messages */
    if(!verbose)
    {
        pos += 2;
    }
    else
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Object Information V1 [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");

        uint16_t num_hdr_msgs = (uint16_t)readField(2, &pos);
        mlog(RAW, "Number of Header Messages:                                       %d\n", (int)num_hdr_msgs);
    }

    /* Read Object Reference Count */
    if(!verbose)
    {
        pos += 4;
    }
    else
    {
        uint32_t obj_ref_count = (uint32_t)readField(4, &pos);
        mlog(RAW, "Object Reference Count:                                          %d\n", (int)obj_ref_count);
    }

    /* Read Object Header Size */
    uint64_t obj_hdr_size = readField(lengthSize, &pos);
    uint64_t end_of_hdr = pos + obj_hdr_size;
    if(verbose)
    {
        mlog(RAW, "Object Header Size:                                              %d\n", (int)obj_hdr_size);
        mlog(RAW, "End of Header:                                                   0x%lx\n", (unsigned long)end_of_hdr);
    }

    /* Read Header Messages */
    pos += readMessagesV1(pos, end_of_hdr, H5LITE_CUSTOM_V1_FLAG, dlvl);

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessagesV1
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessagesV1 (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    static const int SIZE_OF_V1_PREFIX = 8;

    uint64_t starting_position = pos;

    while(pos < (end - SIZE_OF_V1_PREFIX))
    {
        uint16_t    msg_type    = (uint16_t)readField(2, &pos);
        uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;
        
        /* Reserved Bytes */
        if(!errorChecking)
        {
            pos += 3;
        }
        else
        {
            uint8_t  reserved1 = (uint8_t)readField(1, &pos);
            uint16_t reserved2 = (uint16_t)readField(2, &pos);
            if((reserved1 != 0) && (reserved2 != 0))
            {
                mlog(CRITICAL, "invalid reserved fields: %d, %d\n", (int)reserved1, (int)reserved2);
                throw std::runtime_error("invalid reserved fields");
            }
        }

        /* Read Each Message */
        int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);
        if(errorChecking && (bytes_read != msg_size))
        {
            mlog(CRITICAL, "Header message different size than specified: %d != %d\n", bytes_read, msg_size);
            throw std::runtime_error("invalid header message");            
        }

        /* Update Position */
        pos += bytes_read;
    }

    /* Move Past Gap */
    if(pos < end) pos = end;
    
    /* Check Size */
    if(errorChecking)
    {
        if(pos != end)
        {
            mlog(CRITICAL, "Did not read correct number of bytes: %lu != %lu\n", (unsigned long)pos, (unsigned long)end);
            throw std::runtime_error("did not read correct number bytes");            
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessage
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessage (msg_type_t type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    switch(type)
    {
        case DATASPACE_MSG:     return readDataspaceMsg(pos, hdr_flags, dlvl);
        case LINK_INFO_MSG:     return readLinkInfoMsg(pos, hdr_flags, dlvl);
        case DATATYPE_MSG:      return readDatatypeMsg(pos, hdr_flags, dlvl);
        case FILL_VALUE_MSG:    return readFillValueMsg(pos, hdr_flags, dlvl);
        case LINK_MSG:          return readLinkMsg(pos, hdr_flags, dlvl);
        case DATA_LAYOUT_MSG:   return readDataLayoutMsg(pos, hdr_flags, dlvl);
        case FILTER_MSG:        return readFilterMsg(pos, hdr_flags, dlvl);
        case HEADER_CONT_MSG:   return readHeaderContMsg(pos, hdr_flags, dlvl);

        default:
        {
            if(verbose)
            {
                mlog(RAW, "Skipped Message [%d]: 0x%x, %d, 0x%lx\n", dlvl, (int)type, (int)size, (unsigned long)pos);
            }
            
            return size;
        }
    }
}

/*----------------------------------------------------------------------------
 * readDataspaceMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDataspaceMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int MAX_DIM_PRESENT    = 0x1;
    static const int PERM_INDEX_PRESENT = 0x2;

    uint64_t starting_position = pos;

    uint8_t version         = (uint8_t)readField(1, &pos);
    uint8_t dimensionality  = (uint8_t)readField(1, &pos);
    uint8_t flags           = (uint8_t)readField(1, &pos);
    pos += 5; // go past reserved bytes

    if(errorChecking)
    {
        if(version != 1)
        {
            mlog(CRITICAL, "invalid dataspace version: %d\n", (int)version);
            throw std::runtime_error("invalid dataspace version");
        }

        if(flags & PERM_INDEX_PRESENT)
        {
            mlog(CRITICAL, "unsupported permutation indexes\n");
            throw std::runtime_error("unsupported permutation indexes");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Dataspace Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Dimensionality:                                                  %d\n", (int)dimensionality);
        mlog(RAW, "Flags:                                                           0x%x\n", (int)flags);
    }

    /* Read and Populate Data Dimensions */
    dataNumDimensions = dimensionality;
    if(dataNumDimensions > 0)
    {
        dataDimensions = new uint64_t [dataNumDimensions];
        for(int d = 0; d < dataNumDimensions; d++)
        {
            dataDimensions[d] = readField(lengthSize, &pos);
            if(verbose)
            {
                mlog(RAW, "Dimension %d:                                                     %lu\n", (int)dataNumDimensions, (unsigned long)dataDimensions[d]);
            }
        }

        /* Skip Over Maximum Dimensions */
        if(flags & MAX_DIM_PRESENT)
        {
            pos += dataNumDimensions * lengthSize;
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkInfoMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readLinkInfoMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int MAX_CREATE_PRESENT_BIT     = 0x01;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x02;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);
    uint64_t flags = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 0)
        {
            mlog(CRITICAL, "invalid link info version: %d\n", (int)version);
            throw std::runtime_error("invalid link info version");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Information Message [%d], 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }

    /* Read Maximum Creation Index (number of elements in group) */
    if(flags & MAX_CREATE_PRESENT_BIT)
    {
        uint64_t max_create_index = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Maximum Creation Index:                                          %lu\n", (unsigned long)max_create_index);
        }
    }

    /* Read Heap and Name Offsets */
    uint64_t heap_address = readField(offsetSize, &pos);
    uint64_t name_index = readField(offsetSize, &pos);
    if(verbose)
    {
        mlog(RAW, "Heap Address:                                                    %lX\n", (unsigned long)heap_address);
        mlog(RAW, "Name Index:                                                      %lX\n", (unsigned long)name_index);
    }

    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order_index = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Creation Order Index:                                            %lX\n", (unsigned long)create_order_index);
        }
    }

    /* Follow Heap Address if Provided */
    if((int)heap_address != -1)
    {
        readFractalHeap(LINK_MSG, heap_address, hdr_flags, dlvl);
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDatatypeMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDatatypeMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version_class = readField(4, &pos);
    dataElementSize = (int)readField(4, &pos);
    uint64_t version = (version_class & 0xF0) >> 4;
    uint64_t databits = version_class >> 8;

    if(errorChecking)
    {
        if(version != 1)
        {
            mlog(CRITICAL, "invalid datatype version: %d\n", (int)version);
            throw std::runtime_error("invalid datatype version");
        }
    }

    /* Set Data Type */
    dataType = (data_type_t)(version_class & 0x0F);
    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Datatype Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Data Class:                                                      %d, %s\n", (int)dataType, type2str(dataType));
        mlog(RAW, "Data Size:                                                       %d\n", dataElementSize);
    }

    /* Read Data Class Properties */
    switch(dataType)
    {
        case FIXED_POINT_TYPE:
        {
            if(!verbose)
            {
                pos += 4;
            }
            else
            {
                unsigned int byte_order = databits & 0x1;
                unsigned int pad_type = (databits & 0x06) >> 1;
                unsigned int sign_loc = (databits & 0x08) >> 3;

                uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                uint16_t bit_precision  = (uint16_t)readField(2, &pos);

                mlog(RAW, "Byte Order:                                                      %d\n", (int)byte_order);
                mlog(RAW, "Pading Type:                                                     %d\n", (int)pad_type);
                mlog(RAW, "Sign Location:                                                   %d\n", (int)sign_loc);
                mlog(RAW, "Bit Offset:                                                      %d\n", (int)bit_offset);
                mlog(RAW, "Bit Precision:                                                   %d\n", (int)bit_precision);
            }
            pos += 4; // alignment
            break;
        }

        case FLOATING_POINT_TYPE:
        {
            if(!verbose)
            {
                pos += 12;
            }
            else
            {
                unsigned int byte_order = ((databits & 0x40) >> 5) | (databits & 0x1);
                unsigned int pad_type = (databits & 0x0E) >> 1;
                unsigned int mant_norm = (databits & 0x30) >> 4;
                unsigned int sign_loc = (databits & 0xFF00) >> 8;

                uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                uint16_t bit_precision  = (uint16_t)readField(2, &pos);
                uint8_t  exp_location   =  (uint8_t)readField(1, &pos);
                uint8_t  exp_size       =  (uint8_t)readField(1, &pos);
                uint8_t  mant_location  =  (uint8_t)readField(1, &pos);
                uint8_t  mant_size      =  (uint8_t)readField(1, &pos);
                uint32_t exp_bias       = (uint32_t)readField(4, &pos);

                mlog(RAW, "Byte Order:                                                      %d\n", (int)byte_order);
                mlog(RAW, "Pading Type:                                                     %d\n", (int)pad_type);
                mlog(RAW, "Mantissa Normalization:                                          %d\n", (int)mant_norm);
                mlog(RAW, "Sign Location:                                                   %d\n", (int)sign_loc);
                mlog(RAW, "Bit Offset:                                                      %d\n", (int)bit_offset);
                mlog(RAW, "Bit Precision:                                                   %d\n", (int)bit_precision);
                mlog(RAW, "Exponent Location:                                               %d\n", (int)exp_location);
                mlog(RAW, "Exponent Size:                                                   %d\n", (int)exp_size);
                mlog(RAW, "Mantissa Location:                                               %d\n", (int)mant_location);
                mlog(RAW, "Mantissa Size:                                                   %d\n", (int)mant_size);
                mlog(RAW, "Exponent Bias:                                                   %d\n", (int)exp_bias);
            }
            pos += 4; // alignment
            break;
        }

        default: 
        {
            if(errorChecking)
            {
                mlog(CRITICAL, "unsupported datatype: %d\n", (int)dataType);
                throw std::runtime_error("unsupported datatype");
            }
            break;
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readFillValueMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFillValueMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 2)
        {
            mlog(CRITICAL, "invalid fill value version: %d\n", (int)version);
            throw std::runtime_error("invalid fill value version");
        }
    }

    if(!verbose)
    {
        pos += 2;
    }
    else
    {
        uint8_t space_allocation_time = (uint8_t)readField(1, &pos);
        uint8_t fill_value_write_time = (uint8_t)readField(1, &pos);

        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Fill Value Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Space Allocation Time:                                           %d\n", (int)space_allocation_time);
        mlog(RAW, "Fill Value Write Time:                                           %d\n", (int)fill_value_write_time);
    }

    uint8_t fill_value_defined = (uint8_t)readField(1, &pos);
    if(fill_value_defined)
    {
        uint32_t fill_value_size = (uint32_t)readField(4, &pos);
        if(verbose)
        {
            mlog(RAW, "Fill Value Size:                                                 %d\n", (int)fill_value_size);
        }

        if(fill_value_size > 0)
        {
            uint64_t fill_value = readField(fill_value_size, &pos);
            dataFill.fill_ll = fill_value;
            if(verbose)
            {
                mlog(RAW, "Fill Value Size:                                                 %d\n", (int)fill_value_size);
                mlog(RAW, "Fill Value:                                                      0x%llX\n", (unsigned long long)fill_value);
            }
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readLinkMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int SIZE_OF_LEN_OF_NAME_MASK   = 0x03;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x04;
    static const int LINK_TYPE_PRESENT_BIT      = 0x08;
    static const int CHAR_SET_PRESENT_BIT       = 0x10;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);
    uint64_t flags = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 1)
        {
            mlog(CRITICAL, "invalid link version: %d\n", (int)version);
            throw std::runtime_error("invalid link version");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Message [%d]: 0x%x, 0x%lx\n", dlvl, (unsigned)flags, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }

    /* Read Link Type */
    uint8_t link_type = 0; // default to hard link
    if(flags & LINK_TYPE_PRESENT_BIT)
    {
        link_type = readField(1, &pos);
        if(verbose)
        {
            mlog(RAW, "Link Type:                                                       %lu\n", (unsigned long)link_type);
        }
    }

    /* Read Creation Order */
    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Creation Order:                                                  %lX\n", (unsigned long)create_order);
        }
    }

    /* Read Character Set */
    if(flags & CHAR_SET_PRESENT_BIT)
    {
        uint8_t char_set = readField(1, &pos);
        if(verbose)
        {
            mlog(RAW, "Character Set:                                                   %lu\n", (unsigned long)char_set);
        }
    }

    /* Read Link Name */
    int link_name_len_of_len = 1 << (flags & SIZE_OF_LEN_OF_NAME_MASK);
    if(errorChecking && (link_name_len_of_len > 8))
    {
        mlog(CRITICAL, "invalid link name length of length: %d\n", (int)link_name_len_of_len);
        throw std::runtime_error("invalid link name length of length");
    }
    
    uint64_t link_name_len = readField(link_name_len_of_len, &pos);
    if(verbose)
    {
        mlog(RAW, "Link Name Length:                                                %lu\n", (unsigned long)link_name_len);
    }

    uint8_t link_name[STR_BUFF_SIZE];
    readData(link_name, link_name_len, &pos); // plus one for null termination
    link_name[link_name_len] = '\0';
    if(verbose)
    {
        mlog(RAW, "Link Name:                                                       %s\n", link_name);
    }

    /* Process Link Type */
    if(link_type == 0) // hard link
    {
        uint64_t object_header_addr = readField(offsetSize, &pos);
        if(verbose)
        {
            mlog(RAW, "Hard Link - Object Header Address:                               0x%lx\n", object_header_addr);
        }

        if(dlvl < datasetPath.length())
        {
            if(StringLib::match((const char*)link_name, datasetPath[dlvl]))
            {
                readObjHdr(object_header_addr, dlvl + 1);
            }
        }
    }
    else if(link_type == 1) // soft link
    {
        uint16_t soft_link_len = readField(2, &pos);
        uint8_t soft_link[STR_BUFF_SIZE];
        readData(soft_link, soft_link_len, &pos);
        if(verbose)
        {
            mlog(RAW, "Soft Link:                                                       %s\n", soft_link);
        }
    }
    else if(link_type == 64) // external link
    {
        uint16_t ext_link_len = readField(2, &pos);
        uint8_t ext_link[STR_BUFF_SIZE];
        readData(ext_link, ext_link_len, &pos);
        if(verbose)
        {
            mlog(RAW, "External Link:                                                   %s\n", ext_link);
        }
    }
    else if(errorChecking)
    {
        mlog(CRITICAL, "invalid link type: %d\n", link_type);
        throw std::runtime_error("invalid link type");
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDataLayoutMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDataLayoutMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version = readField(1, &pos);
    layout_t layout = (layout_t)readField(1, &pos);

    if(errorChecking)
    {
        if(version != 3)
        {
            mlog(CRITICAL, "invalid data layout version: %d\n", (int)version);
            throw std::runtime_error("invalid data layout version");
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Data Layout Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Layout:                                                          %d, %s\n", (int)layout, layout2str(layout));
    }

    /* Read Layout Classes */
    switch(layout)
    {
        case COMPACT_LAYOUT:
        {
            dataSize = (uint16_t)readField(2, &pos);
            if(dataSize > 0)
            {
                dataBuffer = new uint8_t [dataSize];
                readData(dataBuffer, dataSize, &pos);
            }
            break;
        }

        case CONTIGUOUS_LAYOUT:
        {
            uint64_t data_addr = readField(offsetSize, &pos);
            dataSize = readField(lengthSize, &pos);
            if((dataSize > 0) && (!H5_INVALID(data_addr)))
            {
                dataBuffer = new uint8_t [dataSize];
                readData(dataBuffer, dataSize, &data_addr);
            }
            break;
        }

        case CHUNKED_LAYOUT:
        {
            /* Read Number of Dimensions */
            int chunk_num_dim = (int)readField(1, &pos) - 1; // dimensionality is plus one over actual number of dimensions

            /* Read Address of B-Tree */
            uint64_t data_addr = readField(offsetSize, &pos);

            /* Read Dimensions */
            uint64_t* chunk_dim = NULL;
            if(chunk_num_dim > 0)
            {
                chunk_dim = new uint64_t [chunk_num_dim];
                for(int d = 0; d < chunk_num_dim; d++)
                {
                    chunk_dim[d] = (uint32_t)readField(4, &pos);
                }
            }

            /* Read Size of Data Element */
            int element_size = (int)readField(4, &pos);
            if(errorChecking)
            {
                if(element_size != dataElementSize)
                {
                    mlog(CRITICAL, "chunk element size does not match data element size: %d != %d\n", element_size, dataElementSize);
                    throw std::runtime_error("chunk element size does not match data element size");
                }
            }

            /* Display Data Attributes */
            if(verbose)
            {
                mlog(RAW, "Chunk Element Size:                                              %d\n", (int)element_size);
                mlog(RAW, "Number of Chunked Dimensions:                                    %d\n", (int)chunk_num_dim);
                for(int d = 0; d < dataNumDimensions; d++)
                {
                    mlog(RAW, "Chunk Dimension %d:                                               %d\n", d, (int)chunk_dim[d]);
                }
            }

            /* Check All Parameters Ready */
            if(dataElementSize <= 0 || dataNumDimensions <= 0)
            {
                mlog(CRITICAL, "unable to read data, missing info: %d, %d\n", dataElementSize, dataNumDimensions);
                throw std::runtime_error("unable to read data, missing info");
            }

            /* Allocate Data Buffer */
            dataSize = dataElementSize;
            for(int d = 0; d < dataNumDimensions; d++)
            {
                dataSize *= dataDimensions[d];
            }
            dataBuffer = new uint8_t [dataSize];

            /* Read Data from B-Tree */
            (void)data_addr;
            break;
        }

        default:
        {
            if(errorChecking)
            {
                mlog(CRITICAL, "invalid data layout: %d\n", (int)layout);
                throw std::runtime_error("invalid data layout");
            }
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;
    uint64_t msg_size = ending_position - starting_position;
    if((msg_size % 8) > 0) msg_size += 8 - (msg_size % 8);
    return msg_size;
}

/*----------------------------------------------------------------------------
 * readFilterMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFilterMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version = readField(1, &pos);
    uint32_t num_filters = (uint32_t)readField(1, &pos);
    pos += 6; // move past reserved bytes

    if(errorChecking)
    {
        if(version != 1)
        {
            mlog(CRITICAL, "invalid filter version: %d\n", (int)version);
            throw std::runtime_error("invalid filter version");
        }
    }

    /* Read Filter Description */
    dataFilter                  = (filter_t)readField(2, &pos);
    uint16_t name_len           = (uint16_t)readField(2, &pos);
    uint16_t flags              = (uint16_t)readField(2, &pos);
    dataNumFilterParms          = (int)readField(2, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Filter Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Number of Filters:                                               %d\n", (int)num_filters);
        mlog(RAW, "Filter Identification Value:                                     %d\n", (int)dataFilter);
        mlog(RAW, "Flags:                                                           0x%x\n", (int)flags);
        mlog(RAW, "Number Client Data Values:                                       %d\n", (int)dataNumFilterParms);
    }

    /* Read Name */
    uint8_t filter_name[STR_BUFF_SIZE];
    readData(filter_name, name_len, &pos);
    filter_name[name_len] = '\0';
    if(verbose)
    {
        mlog(RAW, "Filter Name:                                                     %s\n", filter_name);
    }

    /* Client Data */
    uint32_t client_data_size = dataNumFilterParms * 4;
    if(client_data_size)
    {
        dataFilterParms = new uint32_t [dataNumFilterParms];
        readData((uint8_t*)dataFilterParms, client_data_size, &pos);
    }
    else
    {
        pos += client_data_size;
    }

    /* Handle Padding */
    if(dataNumFilterParms % 2 == 1)
    {
        pos += 4;
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readHeaderContMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readHeaderContMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    uint64_t starting_position = pos;

    /* Continuation Info */
    uint64_t hc_offset = readField(offsetSize, &pos);
    uint64_t hc_length = readField(lengthSize, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Header Continuation Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Offset:                                                          0x%lx\n", (unsigned long)hc_offset);
        mlog(RAW, "Length:                                                          %lu\n", (unsigned long)hc_length);
    }

    /* Read Continuation Block */
    pos = hc_offset; // go to continuation block
    if(hdr_flags & H5LITE_CUSTOM_V1_FLAG)
    {
       uint64_t end_of_chdr = hc_offset + hc_length;
       pos += readMessagesV1 (pos, end_of_chdr, hdr_flags, dlvl);
    }
    else
    {
        /* Read Continuation Header */
        if(errorChecking)
        {
            uint64_t signature = readField(4, &pos);
            if(signature != H5_OCHK_SIGNATURE_LE)
            {
                mlog(CRITICAL, "invalid header continuation signature: 0x%llX\n", (unsigned long long)signature);
                throw std::runtime_error("invalid header continuation signature");
            }
        }

        /* Read Continuation Header Messages */
        uint64_t end_of_chdr = hc_offset + hc_length - 4; // leave 4 bytes for checksum below
        pos += readMessages (pos, end_of_chdr, hdr_flags, dlvl);

        /* Verify Checksum */
        uint64_t check_sum = readField(4, &pos);
        if(errorChecking)
        {
            (void)check_sum;
        }
    }

    /* Return Bytes Read */
    return offsetSize + lengthSize;
}

/******************************************************************************
 * HDF5 LITE LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Lite::init (void)
{

}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Lite::deinit (void)
{

}

/*----------------------------------------------------------------------------
 * parseUrl
 *----------------------------------------------------------------------------*/
H5Lite::driver_t H5Lite::parseUrl (const char* url, const char** resource)
{
    /* Sanity Check Input */
    if(!url) return UNKNOWN;

    /* Set Resource */
    if(resource) 
    {
        const char* rptr = StringLib::find(url, "//");
        if(rptr)
        {
            *resource = rptr + 2;
        }
    }

    /* Return Driver */
    if(StringLib::find(url, "file://"))
    {
        return FILE;
    }
    else if(StringLib::find(url, "s3://"))
    {
        return S3;
    }
    else if(StringLib::find(url, "hsds://"))    
    {
        return HSDS;
    }
    else
    {
        return UNKNOWN;
    }
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Lite::info_t H5Lite::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows)
{
    (void)valtype;
    (void)col;
    (void)startrow;
    (void)numrows;

    info_t info;
    bool status = false;

    do
    {
        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open Resource */
        H5FileBuffer h5file(resource, datasetname, true, true);

        fileptr_t file = NULL; //H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file == NULL)
        {
            mlog(CRITICAL, "Failed to open resource: %s\n", url);
            break;
        }

        /* Open Dataset */
        int dataset = 0; // H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", datasetname);
            break;
        }

        /* Set Info */
        uint8_t* data = NULL;
        int elements = 0;
        int typesize = 0;
        long datasize = 0;

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s\n", elements, datasize, url, datasetname);
        uint32_t parent_trace_id = TraceLib::grabId();
        uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lite_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

        /* Stop Trace */
        stop_trace(trace_id);

        /* Read Dataset */
        status = true;
        mlog(CRITICAL, "Failed to read data from %s\n", datasetname);


        /* Return Info */
        info.elements = elements;
        info.typesize = typesize;
        info.datasize = datasize;
        info.data = data;
    }
    while(false);


    /* Return Info */
    if(status)  return info;
    else        throw std::runtime_error("H5Lite");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lite::traverse (const char* url, int max_depth, const char* start_group)
{
    (void)max_depth;
 
    bool status = true;

    try
    {
        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            throw std::runtime_error("Invalid url");
        }

        /* Open File */
        H5FileBuffer h5file(resource, start_group, true, true);
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to traverse resource: %s\n", e.what());
    }

    /* Return Status */
    return status;
}
