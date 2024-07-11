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

#ifndef __h5future__
#define __h5future__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "RecordObject.h"

/******************************************************************************
 * H5FUTURE CLASS
 ******************************************************************************/

class H5Future
{
    public:

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t                    elements;   // number of elements in dataset
            uint32_t                    typesize;   // number of bytes per element
            uint64_t                    datasize;   // total number of bytes in dataset
            uint8_t*                    data;       // point to allocated data buffer
            RecordObject::fieldType_t   datatype;   // data type of elements
            int                         numcols;    // number of columns - anything past the second dimension is grouped together
            int                         numrows;    // number of rows - includes all dimensions after the first as a single row
        } info_t;

        typedef enum {
            INVALID     = -1,
            TIMEOUT     = 0,
            COMPLETE    = 1
        } rc_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                H5Future        (void);
                ~H5Future       (void);

        rc_t    wait            (int timeout); // ms
        void    finish          (bool _valid);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        info_t      info;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool        valid;      // set to false when error encountered
        bool        complete;   // set to true when data fully populated
        Cond        sync;       // signals when data read is complete
};

#endif