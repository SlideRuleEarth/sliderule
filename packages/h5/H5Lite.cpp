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
H5FileBuffer::H5FileBuffer (const char* filename, const char* _dataset, bool _error_checking, bool _verbose)
{
    assert(filename);
    assert(_dataset);

    errorChecking = _error_checking;
    verbose = _verbose;

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

    /* Initialize */
    buffSize = 0;
    currBuffPosition = 0;
    currBuffOffset = 0;
    offsetSize = 0;
    lengthSize = 0;
    groupLeafNodeK = 0;
    groupInternalNodeK = 0;
    rootGroupOffset = 0;
    
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
    if(!readObjHdr(rootGroupOffset))
    {
        mlog(CRITICAL, "Failed to find dataset: %s\n", dataset);
        throw std::runtime_error("failed to find dataset");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::~H5FileBuffer (void)
{
    fclose(fp);
}

/*----------------------------------------------------------------------------
 * getCurrPos
 *----------------------------------------------------------------------------*/
void H5FileBuffer::parseDataset (const char* _dataset)
{
    /* Create Copy of Dataset */
    dataset = StringLib::duplicate(_dataset);

    /* Initialize Level to Zero */
    datasetLevel = 0;
    
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
}

/*----------------------------------------------------------------------------
 * getCurrPos
 *----------------------------------------------------------------------------*/
int64_t H5FileBuffer::getCurrPos (void)
{
    return currBuffPosition + currBuffOffset;
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readField (int size, int64_t pos)
{
    /* Set Field Size */
    int field_size;
    if      (size == USE_OFFSET_SIZE)   field_size = offsetSize;
    else if (size == USE_LENGTH_SIZE)   field_size = lengthSize;
    else if (size > 0)                  field_size = size;
    else throw std::runtime_error("size of field cannot be negative");

    /* Set Field Position */
    int64_t field_position;
    if      (pos == USE_CURRENT_POSITION)   field_position = currBuffPosition + currBuffOffset;
    else if (pos > 0)                       field_position = pos;
    else throw std::runtime_error("position of field cannot be negative");

    /* Check if Different Data Needs to be Buffered */
    if((field_position < currBuffPosition) || ((field_position + field_size) > (currBuffPosition + buffSize)))
    {
        if(fseek(fp, field_position, SEEK_SET) == 0)
        {
            buffSize = fread(buffer, 1, READ_BUFSIZE, fp);
            currBuffOffset = 0;
            currBuffPosition = field_position;
        }
        else
        {
            throw std::runtime_error("failed to go to field position");
        }
    }

    /* Set Buffer Position */
    currBuffOffset = field_position - currBuffPosition;

    /*  Read Field Value */
    uint64_t value;
    switch(field_size)
    {
        case 8:     
        {
            value = *(uint64_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swapll(value);
            #endif
            break;
        }

        case 4:     
        {
            value = *(uint32_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *(uint16_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = buffer[currBuffOffset];
            break;
        }

        default:
        {
            assert(0);
            throw std::runtime_error("invalid field size");
        }
    }

    /* Increment Current Buffer Position */
    currBuffOffset += field_size;

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * readData
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readData (uint8_t* data, uint64_t size, uint64_t pos)
{
    assert(data);
    assert(size > 0);
    assert(pos > 0);

    /* Set Data Position */
    if(fseek(fp, pos, SEEK_SET) == 0)
    {
        currBuffOffset = 0;
        currBuffPosition = pos;
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
        currBuffOffset += buffSize;
    }
}

/*----------------------------------------------------------------------------
 * readObjHdr
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readObjHdr (int64_t pos)
{
    static const int OBJ_HDR_FLAG_SIZE_OF_CHUNK_0_MASK      = 0x03;
    static const int OBJ_HDR_FLAG_ATTR_CREATION_TRACK_BIT   = 0x04;
//    static const int OBJ_HDR_FLAG_ATTR_CREATION_INDEX_BIT   = 0x08;
    static const int OBJ_HDR_FLAG_STORE_CHANGE_PHASE_BIT    = 0x10;
    static const int OBJ_HDR_FLAG_FILE_STATS_BIT            = 0x20;

    /* Read Object Header */
    uint64_t signature = readField(4, pos);
    uint64_t version = readField(1);
    if(errorChecking)
    {
        if(signature != H5_OHDR_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid header signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid header signature");
        }

        if(version != 2)
        {
            mlog(CRITICAL, "invalid header version: %d\n", (int)version);
            throw std::runtime_error("invalid header version");
        }
    }

    /* Read Option Time Fields */
    uint8_t obj_hdr_flags = (uint8_t)readField(1);
    if(obj_hdr_flags & OBJ_HDR_FLAG_FILE_STATS_BIT)
    {
        uint64_t access_time         = readField(4);
        uint64_t modification_time   = readField(4);
        uint64_t change_time         = readField(4);
        uint64_t birth_time          = readField(4);

        if(verbose)
        {
            mlog(RAW, "\n----------------\n");
            mlog(RAW, "Object Information [%d]\n", datasetLevel);
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
    if(obj_hdr_flags & OBJ_HDR_FLAG_STORE_CHANGE_PHASE_BIT)
    {
        uint64_t max_compact_attr = readField(2); (void)max_compact_attr;
        uint64_t max_dense_attr = readField(2); (void)max_dense_attr;
    }

    /* Read Header Messages */
    uint64_t size_of_chunk0 = readField(1 << (obj_hdr_flags & OBJ_HDR_FLAG_SIZE_OF_CHUNK_0_MASK));
    uint64_t chunk0_data_read = 0;
    while(chunk0_data_read < size_of_chunk0)
    {
        uint8_t     hdr_msg_type    = (uint8_t)readField(1);
        uint16_t    hdr_msg_size    = (uint16_t)readField(2);
        uint8_t     hdr_msg_flags   = (uint8_t)readField(1); (void)hdr_msg_flags;
        chunk0_data_read += 4;

        if(obj_hdr_flags & OBJ_HDR_FLAG_ATTR_CREATION_TRACK_BIT)
        {
            uint64_t hdr_msg_order = (uint8_t)readField(2); (void)hdr_msg_order;
            chunk0_data_read += 2;
        }

        /* Read Each Message */
        chunk0_data_read += hdr_msg_size;
        if(readMessage((msg_type_t)hdr_msg_type, hdr_msg_size, getCurrPos()))
        {
            /* Dataset Found */
            return true;
        }
    }

    /* Verify Checksum */
    uint64_t check_sum = readField(4);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Failed to Find Dataset */
    return false;
}

/*----------------------------------------------------------------------------
 * readMessage
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readMessage (msg_type_t type, uint64_t size, int64_t pos)
{
    if(verbose)
    {
        mlog(RAW, "\n** Message: type=0x%x, size=%d **\n", (int)type, (int)size);
    }

    switch(type)
    {
        case LINK_INFO_MSG: return readLinkInfoMsg(pos);
        case LINK_MSG: return readLinkMsg(pos);
//        case FILTER_MSG: return readFilterMsg(pos);

        default:
        {
            uint8_t hdr_msg_data[0x10000];
            readData(hdr_msg_data, size, pos);
            return false;
        }
    }
}

/*----------------------------------------------------------------------------
 * readLinkInfoMsg
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readLinkInfoMsg (int64_t pos)
{
    static const int MAX_CREATE_PRESENT_BIT     = 0x01;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x02;

    uint64_t version = readField(1, pos);
    uint64_t flags = readField(1);

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
        int dlvl = datasetLevel;
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Information Message [%d]\n", dlvl);
        mlog(RAW, "----------------\n");
    }

    /* Read Maximum Creation Index (number of elements in group) */
    if(flags & MAX_CREATE_PRESENT_BIT)
    {
        uint64_t max_create_index = readField(8);
        if(verbose)
        {
            mlog(RAW, "Maximum Creation Index:                                          %lu\n", (unsigned long)max_create_index);
        }
    }

    /* Read Heap and Name Offsets */
    uint64_t heap_address = readField();
    uint64_t name_index = readField();
    if(verbose)
    {
        mlog(RAW, "Heap Address:                                                    %lX\n", (unsigned long)heap_address);
        mlog(RAW, "Name Index:                                                      %lX\n", (unsigned long)name_index);
    }

    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order_index = readField(8);
        if(verbose)
        {
            mlog(RAW, "Creation Order Index:                                            %lX\n", (unsigned long)create_order_index);
        }
    }

    /* Follow Heap Address if Provided */
    if((int)heap_address != -1)
    {
        return readFractalHeap(LINK_MSG, heap_address);
    }

    return false;
}

/*----------------------------------------------------------------------------
 * readLinkMsg
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readLinkMsg (int64_t pos)
{
    static const int SIZE_OF_LEN_OF_NAME_MASK   = 0x03;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x04;
    static const int LINK_TYPE_PRESENT_BIT      = 0x08;
    static const int CHAR_SET_PRESENT_BIT       = 0x10;

    uint64_t version = readField(1, pos);
    uint64_t flags = readField(1);

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
        int dlvl = datasetLevel;
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Message [%d]\n", dlvl);
        mlog(RAW, "----------------\n");
    }

    /* Read Link Type */
    uint8_t link_type = 0;
    if(flags & LINK_TYPE_PRESENT_BIT)
    {
        link_type = readField(1);
        if(verbose)
        {
            mlog(RAW, "Link Type:                                                       %lu\n", (unsigned long)link_type);
        }
    }

    /* Read Creation Order */
    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order = readField(8);
        if(verbose)
        {
            mlog(RAW, "Creation Order:                                                  %lX\n", (unsigned long)create_order);
        }
    }

    /* Read Character Set */
    if(flags & CHAR_SET_PRESENT_BIT)
    {
        uint8_t char_set = readField(1);
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
    
    uint64_t link_name_len = readField(link_name_len_of_len);
    if(verbose)
    {
        mlog(RAW, "Link Name Length:                                                %lu\n", (unsigned long)link_name_len);
    }

    uint8_t link_name[512];
    readData(link_name, link_name_len, getCurrPos()); // plus one for null termination
    link_name[link_name_len] = '\0';
    if(verbose)
    {
        mlog(RAW, "Link Name:                                                       %s\n", link_name);
    }

    /* Process Link Type */
    if(link_type == 0) // hard link
    {
        uint64_t object_header_addr = readField(USE_OFFSET_SIZE);
        if(verbose)
        {
            mlog(RAW, "Hard Link - Object Header Address:                               0x%lx\n", object_header_addr);
        }

        if(datasetLevel < datasetPath.length())
        {
            if(StringLib::match((const char*)link_name, datasetPath[datasetLevel]))
            {
                datasetLevel++;
                return readObjHdr(object_header_addr);
            }
        }
    }
    else if(link_type == 1) // soft link
    {
        uint16_t soft_link_len = readField(2);
        uint8_t soft_link[512];
        readData(soft_link, soft_link_len, getCurrPos());
        if(verbose)
        {
            mlog(RAW, "Soft Link:                                                       %s\n", soft_link);
        }
    }
    else if(link_type == 64) // external link
    {
        uint16_t ext_link_len = readField(2);
        uint8_t ext_link[512];
        readData(ext_link, ext_link_len, getCurrPos());
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

    /* Link NOT in Dataset Path */
    return false;
}

/*----------------------------------------------------------------------------
 * readFilterMsg
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readFilterMsg (int64_t pos)
{
    (void)pos;
    (void)errorChecking;
    (void)verbose;

    /* Unimplemented */
    return false;
}

/*----------------------------------------------------------------------------
 * readSuperblock
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readSuperblock (void)
{
    /* Read and Verify Superblock Info */
    if(errorChecking)
    {
        uint64_t signature = readField(8);
        if(signature != H5_SIGNATURE_LE)
        {
            mlog(CRITICAL, "Invalid h5 file signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid signature");
        }

        uint64_t superblock_version = readField(1);
        if(superblock_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file superblock version: %d\n", (int)superblock_version);
            throw std::runtime_error("invalid superblock version");
        }

        uint64_t freespace_version = readField(1);
        if(freespace_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file free space version: %d\n", (int)freespace_version);
            throw std::runtime_error("invalid free space version");
        }

        uint64_t roottable_version = readField(1);
        if(roottable_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file root table version: %d\n", (int)roottable_version);
            throw std::runtime_error("invalid root table version");
        }

        uint64_t headermsg_version = readField(1);
        if(headermsg_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file header message version: %d\n", (int)headermsg_version);
            throw std::runtime_error("invalid header message version");
        }
    }

    /* Read Size and Root Info */
    offsetSize          = readField(1, 13);
    lengthSize          = readField(1, 14);
    groupLeafNodeK      = readField(2, 16);
    groupInternalNodeK  = readField(2, 18);
    rootGroupOffset     = readField(USE_OFFSET_SIZE, 64);

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
}

/*----------------------------------------------------------------------------
 * readFractalHeap
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readFractalHeap (msg_type_t type, int64_t pos)
{
//    static const int FRHP_HUGE_OBJ_WRAP             = 0x01;
    static const int FRHP_CHECKSUM_DIRECT_BLOCKS    = 0x02;

    uint32_t signature = (uint32_t)readField(4, pos);
    uint8_t  version   =  (uint8_t)readField(1);
    if(errorChecking)
    {
        if(signature != H5_FRHP_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid heap signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid heap signature");
        }

        if(version != 0)
        {
            mlog(CRITICAL, "invalid heap version: %d\n", (int)version);
            throw std::runtime_error("invalid heap version");
        }
    }

    if(verbose)
    {
        int dlvl = datasetLevel;
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Fractal Heap [%d,%d]\n", dlvl, (int)type);
        mlog(RAW, "----------------\n");
    }

    /*  Read Fractal Heap Header */
    uint16_t    heap_obj_id_len     = (uint16_t)readField(2); // Heap ID Length
    uint16_t    io_filter_len       = (uint16_t)readField(2); // I/O Filters' Encoded Length
    uint8_t     flags               =  (uint8_t)readField(1); // Flags
    uint32_t    max_size_mg_obj     = (uint32_t)readField(4); // Maximum Size of Managed Objects
    uint64_t    next_huge_obj_id    = (uint64_t)readField(USE_LENGTH_SIZE); // Next Huge Object ID
    uint64_t    btree_addr_huge_obj = (uint64_t)readField(USE_OFFSET_SIZE); // v2 B-tree Address of Huge Objects
    uint64_t    free_space_mg_blks  = (uint64_t)readField(USE_LENGTH_SIZE); // Amount of Free Space in Managed Blocks
    uint64_t    addr_free_space_mg  = (uint64_t)readField(USE_OFFSET_SIZE); // Address of Managed Block Free Space Manager
    uint64_t    mg_space            = (uint64_t)readField(USE_LENGTH_SIZE); // Amount of Manged Space in Heap
    uint64_t    alloc_mg_space      = (uint64_t)readField(USE_LENGTH_SIZE); // Amount of Allocated Managed Space in Heap
    uint64_t    dblk_alloc_iter     = (uint64_t)readField(USE_LENGTH_SIZE); // Offset of Direct Block Allocation Iterator in Managed Space
    uint64_t    mg_objs             = (uint64_t)readField(USE_LENGTH_SIZE); // Number of Managed Objects in Heap
    uint64_t    huge_obj_size       = (uint64_t)readField(USE_LENGTH_SIZE); // Size of Huge Objects in Heap
    uint64_t    huge_objs           = (uint64_t)readField(USE_LENGTH_SIZE); // Number of Huge Objects in Heap
    uint64_t    tiny_obj_size       = (uint64_t)readField(USE_LENGTH_SIZE); // Size of Tiny Objects in Heap
    uint64_t    tiny_objs           = (uint64_t)readField(USE_LENGTH_SIZE); // Number of Timing Objects in Heap
    uint16_t    table_width         = (uint16_t)readField(2); // Table Width
    uint64_t    starting_blk_size   = (uint64_t)readField(USE_LENGTH_SIZE); // Starting Block Size
    uint64_t    max_dblk_size       = (uint64_t)readField(USE_LENGTH_SIZE); // Maximum Direct Block Size
    uint16_t    max_heap_size       = (uint16_t)readField(2); // Maximum Heap Size
    uint16_t    start_num_rows      = (uint16_t)readField(2); // Starting # of Rows in Root Indirect Block
    uint64_t    root_blk_addr       = (uint64_t)readField(USE_OFFSET_SIZE); // Address of Root Block
    uint16_t    curr_num_rows       = (uint16_t)readField(2); // Current # of Rows in Root Indirect Block

    /* Read Filter Information */
    if(io_filter_len > 0)
    {
        uint64_t filter_root_dblk   = (uint64_t)readField(USE_LENGTH_SIZE); // Size of Filtered Root Direct Block
        uint32_t filter_mask        = (uint32_t)readField(4); // I/O Filter Mask
        mlog(RAW, "Size of Filtered Root Direct Block:                              %lu\n", (unsigned long)filter_root_dblk);
        mlog(RAW, "I/O Filter Mask:                                                 %lu\n", (unsigned long)filter_mask);

        readMessage(FILTER_MSG, io_filter_len, getCurrPos());
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
    uint64_t check_sum = readField(4);
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
        return readDirectBlock(blk_offset_sz, checksum_present, blk_size, mg_objs, type, root_blk_addr);
    }

    /* Heap NOT in Dataset Path */
    return false;
}

/*----------------------------------------------------------------------------
 * readDirectBlock
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::readDirectBlock (int blk_offset_size, bool checksum_present, int blk_size, int msgs_in_blk, msg_type_t type, int64_t pos)
{
    uint32_t signature = (uint32_t)readField(4, pos);
    uint8_t  version   =  (uint8_t)readField(1);
    if(errorChecking)
    {
        if(signature != H5_FHDB_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid direct block signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid direct block signature");
        }

        if(version != 0)
        {
            mlog(CRITICAL, "invalid direct block version: %d\n", (int)version);
            throw std::runtime_error("invalid direct block version");
        }
    }

    if(verbose)
    {
        int dlvl = datasetLevel;
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Direct Block [%d,%d]\n", dlvl, (int)type);
        mlog(RAW, "----------------\n");
    }
    
    /* Read Block Header */
    uint64_t heap_hdr_addr = (uint64_t)readField(USE_OFFSET_SIZE); // Heap Header Address
    uint64_t blk_offset    = (uint64_t)readField(blk_offset_size); // Block Offset
    if(verbose)
    {
        mlog(RAW, "Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        mlog(RAW, "Block Offset:                                                    0x%lx\n", blk_offset);
    }

    if(checksum_present)
    {
        uint64_t check_sum = readField(4);
        if(errorChecking)
        {
            (void)check_sum;
        }
    }

    /* Read Block Data */
    int data_left = blk_size - (5 + offsetSize + blk_offset_size + ((int)checksum_present * 4));
    for(int i = 0; i < msgs_in_blk && data_left > 0; i++)
    {
        int64_t start_pos = getCurrPos();
        if(readMessage(type, data_left, start_pos))
        {
            /* Dataset Found */
            return true;
        }
        int64_t end_pos = getCurrPos();
        data_left -= end_pos - start_pos;
    }

    /* Direct Block NOT in Dataset Path */
    return false;
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
