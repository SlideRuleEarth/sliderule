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

#ifndef __h5_lib__
#define __h5_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"

/******************************************************************************
 * HDF5 I/O LIBRARY
 ******************************************************************************/

struct H5Lib
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/
    
    static const long ALL_ROWS = -1;

    /*--------------------------------------------------------------------
     * Types
     *--------------------------------------------------------------------*/

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

    struct context_t
    {
        uint64_t    read_rqsts;
        uint64_t    bytes_read;

        context_t(void)
        {
            read_rqsts = 0;
            bytes_read = 0;
        }
    };
    
    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void     init        (void);
    static void     deinit      (void);
    static info_t   read        (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context=NULL);
    static bool     traverse    (const char* url, int max_depth, const char* start_group);
};

#endif  /* __h5_lib__ */
