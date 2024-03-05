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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <math.h>
#include <float.h>
#include <map>

#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/*
 * Ancillary Field Records
 * 
 *  This record is used to capture a set of different fields in the source granule,
 *  all associated with a single extent id.  For example, if there was an ancillary
 *  field request for fields X, Y, and Z, then this record would hold the values
 *  for X, Y, and Z all in a single record and associate it with the extent.
 */ 
const char* AncillaryFields::ancFieldRecType = "ancfrec.field";
const RecordObject::fieldDef_t AncillaryFields::ancFieldRecDef[] = {
    {"anc_type",        RecordObject::UINT8,    offsetof(field_t, anc_type),                1,  NULL, NATIVE_FLAGS},
    {"field_index",     RecordObject::UINT8,    offsetof(field_t, field_index),             1,  NULL, NATIVE_FLAGS},
    {"datatype",        RecordObject::UINT8,    offsetof(field_t, data_type),               1,  NULL, NATIVE_FLAGS},
    {"value",           RecordObject::UINT8,    offsetof(field_t, value),                   8,  NULL, NATIVE_FLAGS}
};

const char* AncillaryFields::ancFieldArrayRecType = "ancfrec";
const RecordObject::fieldDef_t AncillaryFields::ancFieldArrayRecDef[] = {
    {"extent_id",       RecordObject::UINT64,   offsetof(field_array_t, extent_id),         1,  NULL, NATIVE_FLAGS},
    {"num_fields",      RecordObject::UINT32,   offsetof(field_array_t, num_fields),        1,  NULL, NATIVE_FLAGS},
    {"fields",          RecordObject::USER,     offsetof(field_array_t, fields),            0,  ancFieldRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

/* 
 * Ancillary Element Records
 *
 *  This record is used to capture an array of field values all associated with a single field.
 *  It is primarily used for the ATL03 photon data and things like that where there is a variable
 *  number of values associated with a given field for a given extent.  So wherease the Ancillary
 *  Field Record is multiple fields each with one value; this is multiple values for just one field.
 */ 
const char* AncillaryFields::ancElementRecType = "ancerec";
const RecordObject::fieldDef_t AncillaryFields::ancElementRecDef[] = {
    {"extent_id",       RecordObject::UINT64,   offsetof(element_array_t, extent_id),       1,  NULL, NATIVE_FLAGS},
    {"num_elements",    RecordObject::UINT32,   offsetof(element_array_t, num_elements),    1,  NULL, NATIVE_FLAGS},
    {"anc_type",        RecordObject::UINT8,    offsetof(element_array_t, anc_type),        1,  NULL, NATIVE_FLAGS},
    {"field_index",     RecordObject::UINT8,    offsetof(element_array_t, field_index),     1,  NULL, NATIVE_FLAGS},
    {"datatype",        RecordObject::UINT8,    offsetof(element_array_t, data_type),       1,  NULL, NATIVE_FLAGS},
    {"data",            RecordObject::UINT8,    offsetof(element_array_t, data),            0,  NULL, NATIVE_FLAGS} // variable length
};

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void AncillaryFields::init (void)
{
    RECDEF(ancFieldRecType,         ancFieldRecDef,         sizeof(field_t),                    NULL);
    RECDEF(ancFieldArrayRecType,    ancFieldArrayRecDef,    offsetof(field_array_t, fields),    NULL);
    RECDEF(ancElementRecType,       ancElementRecDef,       offsetof(element_array_t, data),    NULL);
}

/*----------------------------------------------------------------------------
 * extractAsDoubles
 *----------------------------------------------------------------------------*/
double* AncillaryFields::extractAsDoubles (element_array_t* elements)
{
    double* dst = NULL;
    if(elements->num_elements > 0) dst = new double[elements->num_elements];

    switch(elements->data_type)
    {
        case RecordObject::INT8:
        {
            int8_t* src = (int8_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::INT16:
        {
            int16_t* src = (int16_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::INT32:     
        {
            int32_t* src = (int32_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::INT64:     
        {
            int64_t* src = (int64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::UINT8:     
        {
            uint8_t* src = (uint8_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::UINT16:    
        {
            uint16_t* src = (uint16_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::UINT32:    
        {
            uint32_t* src = (uint32_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::UINT64:    
        {
            uint64_t* src = (uint64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::FLOAT:    
        {
            float* src = getValueAsFloat(&elements->data[0]);
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::DOUBLE:    
        {
            double* src = getValueAsDouble(&elements->data[0]);
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        case RecordObject::TIME8:     
        {
            int64_t* src = (int64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (double)src[i];
            break;
        }
        default:        
        {
            break; // unable to extract
        }
    }

    return dst;
}

/*----------------------------------------------------------------------------
 * extractAsIntegers
 *----------------------------------------------------------------------------*/
int64_t* AncillaryFields::extractAsIntegers (element_array_t* elements)
{
    int64_t* dst = NULL;
    if(elements->num_elements > 0) dst = new int64_t[elements->num_elements];

    switch(elements->data_type)
    {
        case RecordObject::INT8:
        {
            int8_t* src = (int8_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::INT16:
        {
            int16_t* src = (int16_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::INT32:     
        {
            int32_t* src = (int32_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::INT64:     
        {
            int64_t* src = (int64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::UINT8:     
        {
            uint8_t* src = (uint8_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::UINT16:    
        {
            uint16_t* src = (uint16_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::UINT32:    
        {
            uint32_t* src = (uint32_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::UINT64:    
        {
            uint64_t* src = (uint64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::FLOAT:    
        {
            float* src = getValueAsFloat(&elements->data[0]);
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::DOUBLE:    
        {
            double* src = getValueAsDouble(&elements->data[0]);
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        case RecordObject::TIME8:     
        {
            int64_t* src = (int64_t*)&elements->data[0];
            for(uint32_t i = 0; i < elements->num_elements; i++) dst[i] = (int64_t)src[i];
            break;
        }
        default:        
        {
            break; // unable to extract
        }
    }

    return dst;
}

/*----------------------------------------------------------------------------
 * setValueAsDouble
 *----------------------------------------------------------------------------*/
void AncillaryFields::setValueAsDouble (field_t* field, double value)
{
    field->data_type = RecordObject::DOUBLE;
    double* valptr = getValueAsDouble(&field->value[0]);
    *valptr = value;
}

/*----------------------------------------------------------------------------
 * setValueAsInteger
 *----------------------------------------------------------------------------*/
void AncillaryFields::setValueAsInteger (field_t* field, int64_t value)
{
    field->data_type = RecordObject::INT64;
    int64_t* valptr = (int64_t*)&field->value[0];
    *valptr = value;
}

/*----------------------------------------------------------------------------
 * getValueAsDouble
 *----------------------------------------------------------------------------*/
double* AncillaryFields::getValueAsDouble (uint8_t* buffer)
{
    union {
        double* dptr;
        uint8_t* iptr;
    } cast;
    cast.iptr = buffer;
    return cast.dptr;
}

/*----------------------------------------------------------------------------
 * getValueAsFloat
 *----------------------------------------------------------------------------*/
float* AncillaryFields::getValueAsFloat (uint8_t* buffer)
{
    union {
        float* fptr;
        uint8_t* iptr;
    } cast;
    cast.iptr = buffer;
    return cast.fptr;
}

/*----------------------------------------------------------------------------
 * getValueAsInteger
 *----------------------------------------------------------------------------*/
int64_t* AncillaryFields::getValueAsInteger (uint8_t* buffer)
{
    union {
        int64_t* lptr;
        uint8_t* iptr;
    } cast;
    cast.iptr = buffer;
    return cast.lptr;
}

/*----------------------------------------------------------------------------
 * createFieldArrayRecord
 *----------------------------------------------------------------------------*/
RecordObject* AncillaryFields::createFieldArrayRecord (uint64_t extent_id, vector<field_t>& field_vec)
{
    if(field_vec.empty()) return NULL;

    int rec_size = offsetof(field_array_t, fields) + (sizeof(field_t) * field_vec.size());
    RecordObject* rec = new RecordObject(ancFieldArrayRecType, rec_size);
    field_array_t* field_array = (field_array_t*)rec->getRecordData();
    field_array->extent_id = extent_id;
    field_array->num_fields = field_vec.size();
    for(unsigned int i = 0; i < field_vec.size(); i++)
    {
        field_array->fields[i] = field_vec[i];
    }

    return rec;
}
