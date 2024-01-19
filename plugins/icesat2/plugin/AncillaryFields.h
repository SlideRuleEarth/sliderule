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

#ifndef __ancillary_fields__
#define __ancillary_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <string>

#include "LuaObject.h"
#include "RecordObject.h"
#include "OsApi.h"


/******************************************************************************
 * ATL08 DISPATCH CLASS
 ******************************************************************************/

struct AncillaryFields
{
    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/

    /* Estimation Modes */
    typedef enum {
        NEAREST_NEIGHBOR    = 0,
        INTERPOLATION       = 1
    } estimation_t;

    /* Ancillary Field Entry */
    typedef struct {
        string              field;
        estimation_t        estimation;
    } entry_t;

    /* Ancillary Field Types */
    typedef enum {
        PHOTON_ANC_TYPE     = 0,
        EXTENT_ANC_TYPE     = 1,
        ATL08_ANC_TYPE      = 2,
        ATL06_ANC_TYPE      = 3
    } type_t;

    /* Ancillary Field Record */
    typedef struct {
        uint8_t             anc_type;       // type_t
        uint8_t             field_index;    // position in request parameter list
        uint8_t             data_type;      // RecordObject::fieldType_t
        uint8_t             value[8];
    } field_t;

    /* Ancillary Field Array Record */
    typedef struct {
        uint64_t            extent_id;
        uint32_t            num_fields;
        field_t             fields[];
    } field_array_t;

    /* Ancillary Element Array Record */
    typedef struct {
        uint64_t            extent_id;
        uint32_t            num_elements;
        uint8_t             anc_type;       // type_t
        uint8_t             field_index;    // position in request parameter list
        uint8_t             data_type;      // RecordObject::fieldType_t
        uint8_t             data[];
    } element_array_t;

    /* List of Fields */
    typedef List<entry_t> list_t;

    /*--------------------------------------------------------------------
     * Data
     *--------------------------------------------------------------------*/

    /* Field */
    static const char* ancFieldRecType;
    static const RecordObject::fieldDef_t ancFieldRecDef[];
    
    /* Field Array */
    static const char* ancFieldArrayRecType;
    static const RecordObject::fieldDef_t ancFieldArrayRecDef[];
    
    /* Element Array */
    static const char* ancElementRecType;
    static const RecordObject::fieldDef_t ancElementRecDef[];

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void             init                    (void);
    static double*          extractAsDoubles        (element_array_t* elements);
    static int64_t*         extractAsIntegers       (element_array_t* elements);
    static void             setValueAsDouble        (field_t* field, double value);
    static void             setValueAsInteger       (field_t* field, int64_t value);
    static double*          getValueAsDouble        (uint8_t* buffer);
    static float*           getValueAsFloat         (uint8_t* buffer);
    static RecordObject*    createFieldArrayRecord  (uint64_t extent_id, vector<field_t>& field_vec);
};

#endif  /* __ancillary_fields__ */
