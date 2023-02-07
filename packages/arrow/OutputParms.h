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

#ifndef __output_parms__
#define __output_parms__

/*
 * OutputParms works on batches of records.  It expects the `rec_type` passed
 * into the constructor to be the type that defines each of the column headings,
 * then it expects to receive records that are arrays (or batches) of that record
 * type.  The field defined as an array is transparent to this class - it just
 * expects the record to be a single array.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

/******************************************************************************
 * OUTPUT PARAMETERS CLASS
 ******************************************************************************/

struct OutputParms
{
    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/
    typedef enum {
        NATIVE = 0,
        FEATHER = 1,
        PARQUET = 2,
        CSV = 3,
        UNSUPPORTED = 4
    } format_t;

    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/

    static const char* SELF;
    static const char* PATH;
    static const char* FORMAT;
    static const char* OPEN_ON_COMPLETE;

    /*--------------------------------------------------------------------
     * Data
     *--------------------------------------------------------------------*/

    const char*     path;                           // file system path to the file (includes filename)
    format_t        format;                         // format of the file
    bool            open_on_complete;               // flag to client to open file on completion

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

                OutputParms         (lua_State* L, int index);
                ~OutputParms        (void);

    bool        fromLua             (lua_State* L, int index);
    format_t    str2outputformat    (const char* fmt_str);
};

#endif  /* __output_parms__ */
