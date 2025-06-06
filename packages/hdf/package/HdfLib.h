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

#ifndef __hdf_lib__
#define __hdf_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

struct HdfLib
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/
    
    static const long ALL_ROWS = -1;
    static const long ALL_COLS = -1;

    /*--------------------------------------------------------------------
     * Types
     *--------------------------------------------------------------------*/

    typedef struct {
        int                         elements;   // number of elements in dataset
        int                         typesize;   // number of bytes per element
        int                         datasize;   // total number of bytes in dataset
        RecordObject::fieldType_t   datatype;   // data type of elements
        uint8_t*                    data;       // point to allocated data buffer
    } info_t;
    
    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void                         init            (void);
    static void                         deinit          (void);
    static info_t                       read            (const char* filename, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows);
    static bool                         traverse        (const char* filename, int max_depth, const char* start_group);
    static RecordObject::fieldType_t    h5type2datatype (int h5type, int typesize);
};

#endif  /* __hdf_lib__ */