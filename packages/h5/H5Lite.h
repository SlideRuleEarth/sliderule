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
 * HDF5 FILE BUFFER CLASS
 ******************************************************************************/

class H5FileBuffer
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int64_t    USE_OFFSET_SIZE         = -1;
        static const int64_t    USE_LENGTH_SIZE         = -2;
        static const int64_t    USE_CURRENT_POSITION    = -1;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/
        
        typedef enum {
            LINK_INFO_MSG   = 0x2,
            LINK_MSG        = 0x6,
            FILTER_MSG      = 0xB
        } msg_type_t;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            H5FileBuffer        (const char* filename, const char* _dataset, bool _error_checking=false, bool _verbose=false);
        virtual             ~H5FileBuffer       ();

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long       READ_BUFSIZE                            = 1048576; // 1MB
        static const uint64_t   H5_SIGNATURE_LE                         = 0x0A1A0A0D46444889LL;
        static const uint64_t   H5_OHDR_SIGNATURE_LE                    = 0x5244484FLL; // object header
        static const uint64_t   H5_FRHP_SIGNATURE_LE                    = 0x50485246LL; // fractal heap
        static const uint64_t   H5_FHDB_SIGNATURE_LE                    = 0x42444846LL; // direct block

        /*--------------------------------------------------------------------
         * TypeDefs
         *--------------------------------------------------------------------*/

        typedef struct {
            uint64_t    access_time;
            uint64_t    modification_time;
            uint64_t    change_time;
            uint64_t    birth_time;
        } obj_hdr_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void                parseDataset        (const char* _dataset);
        int64_t             getCurrPos          (void);

        uint64_t            readField           (int size=USE_OFFSET_SIZE, int64_t pos=USE_CURRENT_POSITION);
        void                readData            (uint8_t* data, uint64_t size, uint64_t pos);
        bool                readObjHdr          (int64_t pos);
        
        bool                readMessage         (msg_type_t type, uint64_t size, int64_t pos);
        bool                readLinkInfoMsg     (int64_t pos);
        bool                readLinkMsg         (int64_t pos);
        bool                readFilterMsg       (int64_t pos);
        
        void                readSuperblock      (void);        
        bool                readFractalHeap     (msg_type_t type, int64_t pos);
        bool                readDirectBlock     (int blk_offset_size, bool checksum_present, int blk_size, int msgs_in_blk, msg_type_t type, int64_t pos);
        bool                readIndirectBlock   (int64_t pos);

        
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        fileptr_t           fp;
        const char*         dataset;
        List<const char*>   datasetPath;
        int                 datasetLevel;        
        bool                errorChecking;
        bool                verbose;

        /* Buffer Management */
        uint8_t             buffer[READ_BUFSIZE];
        int64_t             buffSize;
        int64_t             currBuffPosition;
        int64_t             currBuffOffset;

        /* File Meta Attributes */
        int                 offsetSize;
        int                 lengthSize;
        int                 groupLeafNodeK;
        int                 groupInternalNodeK;
        int64_t             rootGroupOffset;
};

/******************************************************************************
 * HDF5 I/O LITE LIBRARY
 ******************************************************************************/

struct H5Lite
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/
    
    static const int MAX_NDIMS = 8;
    static const long ALL_ROWS = -1;

    typedef enum {
        FILE,
        HSDS,
        S3,
        UNKNOWN
    } driver_t;

    typedef struct {
        int elements;   // number of elements in dataset
        int typesize;   // number of bytes per element
        int datasize;   // total number of bytes in dataset
        uint8_t* data;  // point to allocated data buffer
    } info_t;

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void     init        (void);
    static void     deinit      (void);

    static driver_t parseUrl    (const char* url, const char** resource);
    static info_t   read        (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows);
    static bool     traverse    (const char* url, int max_depth, const char* start_group);
};

#endif  /* __h5_lite__ */
