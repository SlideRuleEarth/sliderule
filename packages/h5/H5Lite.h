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

/******************************************************************************
 * HDF5 FILE BUFFER CLASS
 ******************************************************************************/

class H5FileBuffer
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long       READ_BUFSIZE            = 1048576; // 1MB
        static const int64_t    USE_OFFSET_SIZE         = -1;
        static const int64_t    USE_LENGTH_SIZE         = -2;
        static const int64_t    USE_CURRENT_POSITION    = -1;
        static const uint64_t   H5_SIGNATURE_LE         = 0x0A1A0A0D46444889LL;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            H5FileBuffer        (const char* filename, bool error_checking=true);
        virtual             ~H5FileBuffer       ();

        uint64_t            readNextField       (int size=USE_OFFSET_SIZE, int64_t pos=USE_CURRENT_POSITION);

        void                displayFileInfo     (void);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        fileptr_t   fp;

        /* Buffer Management */
        uint8_t     buffer[READ_BUFSIZE];
        int64_t     buffSize;
        int64_t     currFilePos;
        int64_t     currBuffPos;

        /* File Attributes */
        int         offsetSize;
        int         lengthSize;
        int         groupLeafNodeK;
        int         groupInternalNodeK;
        int64_t     rootGroupOffset;
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
