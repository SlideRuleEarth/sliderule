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

/*
 *  String Representation: <rec_type> [[<field>=<value>], ...]
 *  Record Format v1: <rec_type> '\0' [record data]
 *  Record Format v2: <version:2bytes> <rectype length:2bytes> <data length:4bytes> <rec_type> '\0' [record data]
 */

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
#include "StringLib.h"
#include "OsApi.h"
#include "EventLib.h"
#include "Dictionary.h"

#include <math.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

MgDictionary<RecordObject::definition_t*> RecordObject::definitions;
Mutex RecordObject::defMut;

const char* RecordObject::DEFAULT_DOUBLE_FORMAT = "%.6lf";
const char* RecordObject::DEFAULT_LONG_FORMAT = "%ld";
const double RecordObject::FLOAT_MAX_VALUE = 4294967296.0;

const int RecordObject::FIELD_TYPE_BYTES[NUM_FIELD_TYPES] = {
    1, // INT8
    2, // INT16
    4, // INT32
    8, // INT64
    1, // UINT8
    2, // UINT16
    4, // UINT32
    8, // UINT64
    0, // BITFIELD
    4, // FLOAT
    8, // DOUBLE
    8, // TIME8
    1, // STRING
    0, // USER
    0  // INVALID_FIELD
};

/******************************************************************************
 * RECORD OBJECT FIELD METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  offset and size are in bits
 *----------------------------------------------------------------------------*/
RecordObject::Field::Field(RecordObject& _rec, fieldType_t _type, int _offset, int _elements, unsigned int _flags, int _element):
    record(_rec),
    element(_element)
{
    field.type      = _type;
    field.offset    = _offset;
    field.elements  = _elements;
    field.flags     = _flags;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RecordObject::Field::Field(RecordObject& _rec, field_t _field, int _element):
    record(_rec),
    field(_field),
    element(_element)
{
}

/*----------------------------------------------------------------------------
 * Constructor (COPY)
 *----------------------------------------------------------------------------*/
RecordObject::Field::Field(const Field& f):
    record(f.record),
    field(f.field),
    element(f.element)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RecordObject::Field::~Field(void)
{
}

/*----------------------------------------------------------------------------
 * operator= <string>
 *----------------------------------------------------------------------------*/
RecordObject::Field& RecordObject::Field::operator=(const char* const rhs)
{
    record.setValueText(field, rhs);
    return *this;
}

/*----------------------------------------------------------------------------
 * operator= <double>
 *----------------------------------------------------------------------------*/
RecordObject::Field& RecordObject::Field::operator=(double const& rhs)
{
    record.setValueReal(field, rhs, element);
    return *this;
}

/*----------------------------------------------------------------------------
 * operator= <long>
 *----------------------------------------------------------------------------*/
RecordObject::Field& RecordObject::Field::operator=(long const& rhs)
{
    record.setValueInteger(field, rhs, element);
    return *this;
}

/*----------------------------------------------------------------------------
 * getValueText
 *----------------------------------------------------------------------------*/
const char* RecordObject::Field::getValueText(char* valbuf)
{
    return record.getValueText(field, valbuf);
}

/*----------------------------------------------------------------------------
 * getValueReal
 *----------------------------------------------------------------------------*/
double RecordObject::Field::getValueReal(void)
{
    return record.getValueReal(field, element);
}

/*----------------------------------------------------------------------------
 * getValueInteger
 *----------------------------------------------------------------------------*/
long RecordObject::Field::getValueInteger(void)
{
    return record.getValueInteger(field, element);
}

/*----------------------------------------------------------------------------
 * getValueType
 *----------------------------------------------------------------------------*/
RecordObject::valType_t RecordObject::Field::getValueType(void)
{
    return record.getValueType(field);
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Attempts to create record from a string specification
 *----------------------------------------------------------------------------*/
RecordObject::RecordObject(const char* rec_type, int allocated_memory)
{
    assert(rec_type);

    /* Attempt to Get Record Type */
    recordDefinition = getDefinition(rec_type);

    /* Attempt to Initialize Record */
    if(recordDefinition != NULL)
    {
        int data_size;

        /* Calculate Memory to Allocate */
        if(allocated_memory == 0)
        {
            memoryAllocated = recordDefinition->record_size;
            data_size = recordDefinition->data_size;
        }
        else if(allocated_memory + (int)sizeof(rec_hdr_t) + recordDefinition->type_size >= recordDefinition->record_size)
        {
            memoryAllocated = allocated_memory + sizeof(rec_hdr_t) + recordDefinition->type_size;
            data_size = allocated_memory;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid memory allocation in record creation");
        }

        /* Allocate Record Memory */
        memoryOwner = true;
        recordMemory = new char[memoryAllocated];

        /* Populate Header */
        recordData = (unsigned char*)populateHeader(recordMemory, recordDefinition->type_name, recordDefinition->type_size, data_size);

        /* Zero Out Record Data */
        LocalLib::set(recordData, 0, recordDefinition->data_size);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "could not locate record definition %s", rec_type);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Assumes a serialized buffer <type string><binary data>
 *  (another way to perform deserialization)
 *----------------------------------------------------------------------------*/
RecordObject::RecordObject(unsigned char* buffer, int size)
{
    recordDefinition = getDefinition(buffer, size);
    if(recordDefinition != NULL)
    {
        if (size >= recordDefinition->record_size)
        {
            /* Set Record Memory */
            memoryOwner = true;
            memoryAllocated = size;
            recordMemory = new char[memoryAllocated];
            LocalLib::copy(recordMemory, buffer, memoryAllocated);

            /* Set Record Data */
            recordData = (unsigned char*)&recordMemory[sizeof(rec_hdr_t) + recordDefinition->type_size];
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "buffer passed in not large enough to populate record");
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "buffer did not contain defined record");
    }
}

/*----------------------------------------------------------------------------
 * Denstructor
 *----------------------------------------------------------------------------*/
RecordObject::~RecordObject(void)
{
    if(memoryOwner) delete [] recordMemory;
}

/*----------------------------------------------------------------------------
 * deserialize
 *
 *  Notes
 *   1. checks that type matches exactly
 *   2. no new memory is allocated
 *----------------------------------------------------------------------------*/
bool RecordObject::deserialize(unsigned char* buffer, int size)
{
    /* Get Record Definition */
    definition_t* def = getDefinition(buffer, size);
    if(def != recordDefinition)
    {
        return false; // record does not match definition
    }

    /* Check Size */
    if(size > memoryAllocated)
    {
        return false; // buffer passed in too large
    }
    else if(size < def->type_size)
    {
        return false; // buffer not large enough to populate type string
    }

    /* Copy Data and Return */
    LocalLib::copy(recordMemory, buffer, size);
    return true;
}

/*----------------------------------------------------------------------------
 * serialize
 *----------------------------------------------------------------------------*/
int RecordObject::serialize(unsigned char** buffer, serialMode_t mode, int size)
{
    assert(buffer);

    /* Determine Buffer Size */
    int bufsize = memoryAllocated;

    /* Allocate or Copy Buffer */
    if (mode == ALLOCATE)
    {
        *buffer = new unsigned char[bufsize];
        LocalLib::copy(*buffer, recordMemory, bufsize);
    }
    else if (mode == REFERENCE)
    {
        *buffer = (unsigned char*)recordMemory;
    }
    else // if (mode == COPY)
    {
        assert(*buffer);
        bufsize = MIN(bufsize, size);
        LocalLib::copy(*buffer, recordMemory, bufsize);
    }

    /* Return Buffer Size */
    return bufsize;
}

/*----------------------------------------------------------------------------
 * isRecordType
 *----------------------------------------------------------------------------*/
bool RecordObject::isRecordType(const char* rec_type)
{
    return (StringLib::match(rec_type, recordDefinition->type_name));
}

/*----------------------------------------------------------------------------
 * getRecordType
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordType(void)
{
    return recordDefinition->type_name;
}

/*----------------------------------------------------------------------------
 * getRecordId
 *----------------------------------------------------------------------------*/
long RecordObject::getRecordId(void)
{
    if(recordDefinition->id_field)
    {
        field_t f = getField(recordDefinition->id_field);

        if(f.type != INVALID_FIELD)
        {
            return getValueInteger(f);
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * getRecordData
 *----------------------------------------------------------------------------*/
unsigned char* RecordObject::getRecordData(void)
{
    return recordData;
}

/*----------------------------------------------------------------------------
 * getRecordTypeSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordTypeSize(void)
{
    return recordDefinition->type_size;
}

/*----------------------------------------------------------------------------
 * getRecordDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordDataSize(void)
{
    return recordDefinition->data_size;
}

/*----------------------------------------------------------------------------
 * getAllocatedMemory
 *----------------------------------------------------------------------------*/
int RecordObject::getAllocatedMemory(void)
{
    return memoryAllocated;
}

/*----------------------------------------------------------------------------
 * createRecordField
 *----------------------------------------------------------------------------*/
RecordObject::Field* RecordObject::createRecordField(const char* field_name)
{
    RecordField* rec_field = NULL;
    field_t f = getField(field_name);
    if(f.type != INVALID_FIELD)
    {
        rec_field = new RecordField(*this, f.type, f.offset, f.elements, f.flags);
    }

    return rec_field;
}

/*----------------------------------------------------------------------------
 * populate
 *
 *  <field>=<value>
 *----------------------------------------------------------------------------*/
bool RecordObject::populate (const char* populate_string)
{
    bool status = true;

    char(*toks)[MAX_STR_SIZE] = new (char[MAX_INITIALIZERS][MAX_STR_SIZE]);

    int numtoks = StringLib::tokenizeLine(populate_string, (int)StringLib::size(populate_string, MAX_STR_SIZE - 1) + 1, ' ', MAX_INITIALIZERS, toks);

    for(int i = 0; i < numtoks; i++)
    {
        char args[2][MAX_STR_SIZE];
        if(StringLib::tokenizeLine(toks[i], MAX_STR_SIZE, '=', 2, args) == 2)
        {
            char* field_str = args[0];
            char* value_str = args[1];

            field_t f = getField(field_str);
            if(f.type != RecordObject::INVALID_FIELD)
            {
                setValueText(f, value_str);
            }
            else
            {
                status = false;
            }
        }
    }

    delete[] toks;

    return status;
}

/*----------------------------------------------------------------------------
 * setIdField
 *----------------------------------------------------------------------------*/
void RecordObject::setIdField (const char* id_field)
{
    defMut.lock();
    {
        if(recordDefinition->id_field) delete [] recordDefinition->id_field;
        recordDefinition->id_field = StringLib::duplicate(id_field);
    }
    defMut.unlock();
}

/*----------------------------------------------------------------------------
 * getNumFields
 *----------------------------------------------------------------------------*/
int RecordObject::getNumFields(void)
{
    return recordDefinition->fields.length();
}

/*----------------------------------------------------------------------------
 * getFieldNames
 *----------------------------------------------------------------------------*/
int RecordObject::getFieldNames(char*** names)
{
    return recordDefinition->fields.getKeys(names);
}

/*----------------------------------------------------------------------------
 * getField
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::getField(const char* field_name)
{
    if(field_name[0] == IMMEDIATE_FIELD_SYMBOL)
    {
        return parseImmediateField(field_name);
    }
    else
    {
        return getUserField(recordDefinition, field_name);
    }
}

/*----------------------------------------------------------------------------
 * field
 *----------------------------------------------------------------------------*/
RecordObject::Field RecordObject::field(const char* field_name)
{
    return Field(*this, getField(field_name));
}

/*----------------------------------------------------------------------------
 * setValueText
 *
 *  The element parameter is only used when the field is a pointer
 *----------------------------------------------------------------------------*/
void RecordObject::setValueText(field_t f, const char* val, int element)
{
    valType_t val_type = getValueType(f);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, false, element);
        if(val == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Cannot null existing pointer!");
        else            setValueText(ptr_field, val);
    }
    else if(val_type == TEXT)
    {
        int val_len = (int)StringLib::size(val, MAX_VAL_STR_SIZE) + 1;
        if(val_len <= f.elements)
        {
            LocalLib::copy(recordData + TOBYTES(f.offset), (unsigned char*)val, val_len);
        }
        else if(f.elements > 0)
        {
            LocalLib::copy(recordData + TOBYTES(f.offset), (unsigned char*)val, f.elements - 1);
            *(recordData + TOBYTES(f.offset) + f.elements - 1) = '\0';
        }
        else // variable length
        {
            int memory_left = MIN(MAX_VAL_STR_SIZE, memoryAllocated - recordDefinition->type_size - TOBYTES(f.offset));
            if(memory_left > 1)
            {
                LocalLib::copy(recordData + TOBYTES(f.offset), (unsigned char*)val, memory_left - 1);
                *(recordData + TOBYTES(f.offset) + memory_left - 1) = '\0';
            }
        }
    }
    else if(val_type == INTEGER)
    {
        long ival;
        if(StringLib::str2long(val, &ival))
        {
            setValueInteger(f, ival);
        }
    }
    else if(val_type == REAL)
    {
        double dval;
        if(StringLib::str2double(val, &dval))
        {
            setValueReal(f, dval);    // will use overloaded double assigment
        }
    }
}

/*----------------------------------------------------------------------------
 * setValueReal
 *----------------------------------------------------------------------------*/
void RecordObject::setValueReal(field_t f, const double val, int element)
{
    if(element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_ERROR, "Out of range access");
    uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, false, element);
        setValueReal(ptr_field, val, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN)) // architectures match
    {
        switch(f.type)
        {
            case INT8:      *(int8_t*)  (recordData + elem_offset) = (int8_t)val;    break;
            case INT16:     *(int16_t*) (recordData + elem_offset) = (int16_t)val;   break;
            case INT32:     *(int32_t*) (recordData + elem_offset) = (int32_t)val;   break;
            case INT64:     *(int64_t*) (recordData + elem_offset) = (int64_t)val;   break;
            case UINT8:     *(uint8_t*) (recordData + elem_offset) = (uint8_t)val;   break;
            case UINT16:    *(uint16_t*)(recordData + elem_offset) = (uint16_t)val;  break;
            case UINT32:    *(uint32_t*)(recordData + elem_offset) = (uint32_t)val;  break;
            case UINT64:    *(uint64_t*)(recordData + elem_offset) = (uint64_t)val;  break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, (long)val); break;
            case FLOAT:     *(float*) (recordData + elem_offset) = (float)val;   break;
            case DOUBLE:    *(double*)(recordData + elem_offset) = val;          break;
            case TIME8:     {
                                double intpart;
                                uint32_t seconds = (uint32_t)val;
                                uint32_t subseconds = (uint32_t)modf(val , &intpart);
                                *(uint32_t*)(recordData + elem_offset) = seconds;
                                *(uint32_t*)(recordData + elem_offset + 4) = subseconds;
                                break;
                            }
            case STRING:    StringLib::format((char*)(recordData + elem_offset), f.elements, DEFAULT_DOUBLE_FORMAT, val);
                            break;
            default:        break;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      *(int8_t*)  (recordData + elem_offset) = (int8_t)val;                     break;
            case INT16:     *(int16_t*) (recordData + elem_offset) = LocalLib::swaps((int16_t)val);   break;
            case INT32:     *(int32_t*) (recordData + elem_offset) = LocalLib::swapl((int32_t)val);   break;
            case INT64:     *(int64_t*) (recordData + elem_offset) = LocalLib::swapll((int64_t)val);  break;
            case UINT8:     *(uint8_t*) (recordData + elem_offset) = (uint8_t)val;                    break;
            case UINT16:    *(uint16_t*)(recordData + elem_offset) = LocalLib::swaps((uint16_t)val);  break;
            case UINT32:    *(uint32_t*)(recordData + elem_offset) = LocalLib::swapl((uint32_t)val);  break;
            case UINT64:    *(uint64_t*)(recordData + elem_offset) = LocalLib::swapll((uint64_t)val); break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, (long)val);                  break;
            case FLOAT:     *(float*) (recordData + elem_offset) = LocalLib::swapf((float)val);   break;
            case DOUBLE:    *(double*)(recordData + elem_offset) = LocalLib::swaplf((double)val); break;
            case TIME8:     {
                                double intpart;
                                uint32_t seconds = (uint32_t)val;
                                uint32_t subseconds = (uint32_t)modf(val , &intpart);
                                *(uint32_t*)(recordData + elem_offset) = LocalLib::swapl(seconds);
                                *(uint32_t*)(recordData + elem_offset + 4) = LocalLib::swapl(subseconds);
                                break;
                            }
            case STRING:    StringLib::format((char*)(recordData + elem_offset), f.elements, DEFAULT_DOUBLE_FORMAT, val);
                            break;
            default:        break;
        }
    }
}

/*----------------------------------------------------------------------------
 * setValueInteger
 *----------------------------------------------------------------------------*/
void RecordObject::setValueInteger(field_t f, const long val, int element)
{
    if(element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_ERROR, "Out of range access");
    uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, false, element);
        setValueInteger(ptr_field, val, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      *(int8_t*)  (recordData + elem_offset) = (int8_t)val;     break;
            case INT16:     *(int16_t*) (recordData + elem_offset) = (int16_t)val;    break;
            case INT32:     *(int32_t*) (recordData + elem_offset) = (int32_t)val;    break;
            case INT64:     *(int64_t*) (recordData + elem_offset) = (int64_t)val;    break;
            case UINT8:     *(uint8_t*) (recordData + elem_offset) = (uint8_t)val;    break;
            case UINT16:    *(uint16_t*)(recordData + elem_offset) = (uint16_t)val;   break;
            case UINT32:    *(uint32_t*)(recordData + elem_offset) = (uint32_t)val;   break;
            case UINT64:    *(uint64_t*)(recordData + elem_offset) = (uint64_t)val;   break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, (long)val);  break;
            case FLOAT:     *(float*) (recordData + elem_offset) = (float)val;    break;
            case DOUBLE:    *(double*)(recordData + elem_offset) = val;           break;
            case TIME8:     {
                                double intpart;
                                uint32_t seconds = (uint32_t)val;
                                uint32_t subseconds = (uint32_t)modf(val , &intpart);
                                *(uint32_t*)(recordData + elem_offset) = seconds;
                                *(uint32_t*)(recordData + elem_offset + 4) = subseconds;
                                break;
                            }
            case STRING:    StringLib::format((char*)(recordData + elem_offset), f.elements, DEFAULT_LONG_FORMAT, val);
                            break;
            default:        break;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      *(int8_t*)  (recordData + elem_offset) = (int8_t)val;                     break;
            case INT16:     *(int16_t*) (recordData + elem_offset) = LocalLib::swaps((int16_t)val);   break;
            case INT32:     *(int32_t*) (recordData + elem_offset) = LocalLib::swapl((int32_t)val);   break;
            case INT64:     *(int64_t*)(recordData + elem_offset)  = LocalLib::swapll((int64_t)val);  break;
            case UINT8:     *(uint8_t*) (recordData + elem_offset) = (uint8_t)val;                    break;
            case UINT16:    *(uint16_t*)(recordData + elem_offset) = LocalLib::swaps((uint16_t)val);  break;
            case UINT32:    *(uint32_t*)(recordData + elem_offset) = LocalLib::swapl((uint32_t)val);  break;
            case UINT64:    *(uint64_t*)(recordData + elem_offset) = LocalLib::swapll((uint64_t)val); break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, (long)val);                  break;
            case FLOAT:     *(float*) (recordData + elem_offset) = LocalLib::swapf((float)val);   break;
            case DOUBLE:    *(double*)(recordData + elem_offset) = LocalLib::swaplf((double)val); break;
            case TIME8:     {
                                double intpart;
                                uint32_t seconds = (uint32_t)val;
                                uint32_t subseconds = (uint32_t)modf(val , &intpart);
                                *(uint32_t*)(recordData + elem_offset) = LocalLib::swapl(seconds);
                                *(uint32_t*)(recordData + elem_offset + 4) = LocalLib::swapl(subseconds);
                                break;
                            }
            case STRING:    StringLib::format((char*)(recordData + elem_offset), f.elements, DEFAULT_LONG_FORMAT, val);
                            break;
            default:        break;
        }
    }
}

/*----------------------------------------------------------------------------
 * getValueText
 *
 *  Notes:
 *   1. If valbuf supplied, then best effort to return string representation of data
 *   2. If valbuf not supplied, then non-text fields return NULL and text fields
 *      return a pointer to the string as it resides in the record
 *   3. valbuf, if supplied, is assumed to be of length MAX_VAL_STR_SIZE
 *   4. The element parameter is only used if the field is a pointer
 *----------------------------------------------------------------------------*/
const char* RecordObject::getValueText(field_t f, char* valbuf, int element)
{
    valType_t val_type = getValueType(f);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, true, element);
        if(ptr_field.offset == 0)   return NULL;
        else                        return getValueText(ptr_field, valbuf);
    }
    else if(val_type == TEXT)
    {
        char* str = (char*)(recordData + TOBYTES(f.offset));
        if(valbuf)
        {
            if(f.elements > 0)
            {
                return StringLib::copy(valbuf, str, f.elements);
            }
            else // variable length
            {
                int memory_left = MIN(MAX_VAL_STR_SIZE, memoryAllocated - recordDefinition->type_size - TOBYTES(f.offset));
                if(memory_left > 1)
                {
                    return StringLib::copy(valbuf, str, memory_left);
                }
            }
        }
        else // valbuf not supplied
        {
            return str;
        }
    }
    else if(val_type == INTEGER && valbuf)
    {
        long val = getValueInteger(f);
        return StringLib::format(valbuf, MAX_VAL_STR_SIZE, DEFAULT_LONG_FORMAT, val);
    }
    else if(val_type == REAL && valbuf)
    {
        double val = getValueReal(f);
        return StringLib::format(valbuf, MAX_VAL_STR_SIZE, DEFAULT_DOUBLE_FORMAT, val);
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * getValueReal
 *----------------------------------------------------------------------------*/
double RecordObject::getValueReal(field_t f, int element)
{
    if(element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_ERROR, "Out of range access");
    uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, false, element);
        return getValueReal(ptr_field, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      return (double)*(int8_t*)  (recordData + elem_offset);
            case INT16:     return (double)*(int16_t*) (recordData + elem_offset);
            case INT32:     return (double)*(int32_t*) (recordData + elem_offset);
            case INT64:     return (double)*(int64_t*) (recordData + elem_offset);
            case UINT8:     return (double)*(uint8_t*) (recordData + elem_offset);
            case UINT16:    return (double)*(uint16_t*)(recordData + elem_offset);
            case UINT32:    return (double)*(uint32_t*)(recordData + elem_offset);
            case UINT64:    return (double)*(uint64_t*)(recordData + elem_offset);
            case BITFIELD:  return (double)unpackBitField(recordData, f.offset, f.elements);
            case FLOAT:     return (double)*(float*) (recordData + elem_offset);
            case DOUBLE:    return *(double*)(recordData + elem_offset);
            case TIME8:     {
                                uint32_t seconds = *(uint32_t*)(recordData + elem_offset);
                                uint32_t subseconds = *(uint32_t*)(recordData + elem_offset + 4);
                                return ((double)seconds + ((double)subseconds / FLOAT_MAX_VALUE));
                            }
            default:        return 0.0;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      return (double)                 *(int8_t*)  (recordData + elem_offset);
            case INT16:     return (double)LocalLib::swaps (*(int16_t*) (recordData + elem_offset));
            case INT32:     return (double)LocalLib::swapl (*(int32_t*) (recordData + elem_offset));
            case INT64:     return (double)LocalLib::swapll(*(int64_t*) (recordData + elem_offset));
            case UINT8:     return (double)                 *(uint8_t*) (recordData + elem_offset);
            case UINT16:    return (double)LocalLib::swaps (*(uint16_t*)(recordData + elem_offset));
            case UINT32:    return (double)LocalLib::swapl (*(uint32_t*)(recordData + elem_offset));
            case UINT64:    return (double)LocalLib::swapll(*(uint64_t*)(recordData + elem_offset));
            case BITFIELD:  return (double)unpackBitField(recordData, f.offset, f.elements);
            case FLOAT:     return (double)LocalLib::swapf (*(float*) (recordData + elem_offset));
            case DOUBLE:    return (double)LocalLib::swaplf(*(double*)(recordData + elem_offset));
            case TIME8:     {
                                uint32_t seconds = LocalLib::swapl(*(uint32_t*)(recordData + elem_offset));
                                uint32_t subseconds = LocalLib::swapl(*(uint32_t*)(recordData + elem_offset + 4));
                                return (double)((double)seconds + ((double)subseconds / FLOAT_MAX_VALUE));
                            }
            default:        return 0.0;
        }
    }
}

/*----------------------------------------------------------------------------
 * getValueInteger
 *----------------------------------------------------------------------------*/
long RecordObject::getValueInteger(field_t f, int element)
{
    if(element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_ERROR, "Out of range access");
    uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);

    if(f.flags & POINTER)
    {
        field_t ptr_field = getPointedToField(f, false, element);
        return getValueInteger(ptr_field, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      return (long)*(int8_t*)  (recordData + elem_offset);
            case INT16:     return (long)*(int16_t*) (recordData + elem_offset);
            case INT32:     return (long)*(int32_t*) (recordData + elem_offset);
            case INT64:     return (long)*(int64_t*) (recordData + elem_offset);
            case UINT8:     return (long)*(uint8_t*) (recordData + elem_offset);
            case UINT16:    return (long)*(uint16_t*)(recordData + elem_offset);
            case UINT32:    return (long)*(uint32_t*)(recordData + elem_offset);
            case UINT64:    return (long)*(uint64_t*)(recordData + elem_offset);
            case BITFIELD:  return (long)unpackBitField(recordData, f.offset, f.elements);
            case FLOAT:     return (long)*(float*) (recordData + elem_offset);
            case DOUBLE:    return (long)*(double*)(recordData + elem_offset);
            case TIME8:     {
                                uint32_t seconds = *(uint32_t*)(recordData + elem_offset);
                                uint32_t subseconds = *(uint32_t*)(recordData + elem_offset + 4);
                                return (long)((double)seconds + ((double)subseconds / FLOAT_MAX_VALUE));
                            }
            default:        return 0;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      return (long)                 *(int8_t*)  (recordData + elem_offset);
            case INT16:     return (long)LocalLib::swaps (*(int16_t*) (recordData + elem_offset));
            case INT32:     return (long)LocalLib::swapl (*(int32_t*) (recordData + elem_offset));
            case INT64:     return (long)LocalLib::swapll(*(int64_t*) (recordData + elem_offset));
            case UINT8:     return (long)                 *(uint8_t*) (recordData + elem_offset);
            case UINT16:    return (long)LocalLib::swaps (*(uint16_t*)(recordData + elem_offset));
            case UINT32:    return (long)LocalLib::swapl (*(uint32_t*)(recordData + elem_offset));
            case UINT64:    return (long)LocalLib::swapll(*(uint64_t*)(recordData + elem_offset));
            case BITFIELD:  return (long)unpackBitField(recordData, f.offset, f.elements);
            case FLOAT:     return (long)LocalLib::swapf (*(float*) (recordData + elem_offset));
            case DOUBLE:    return (long)LocalLib::swaplf(*(double*)(recordData + elem_offset));
            case TIME8:     {
                                uint32_t seconds = LocalLib::swapl(*(uint32_t*)(recordData + elem_offset));
                                uint32_t subseconds = LocalLib::swapl(*(uint32_t*)(recordData + elem_offset + 4));
                                return (long)((double)seconds + ((double)subseconds / FLOAT_MAX_VALUE));
                            }
            default:        return 0;
        }
    }
}

/*----------------------------------------------------------------------------
 * getValueType
 *----------------------------------------------------------------------------*/
RecordObject::valType_t RecordObject::getValueType(field_t f)
{
    switch(f.type)
    {
        case INT8:      return INTEGER;
        case INT16:     return INTEGER;
        case INT32:     return INTEGER;
        case INT64:     return INTEGER;
        case UINT8:     return INTEGER;
        case UINT16:    return INTEGER;
        case UINT32:    return INTEGER;
        case UINT64:    return INTEGER;
        case BITFIELD:  return INTEGER;
        case FLOAT:     return REAL;
        case DOUBLE:    return REAL;
        case TIME8:     return REAL;
        case STRING:    return TEXT;
        case USER:      return DYNAMIC;
        default:        return DYNAMIC;
    }
}

/*----------------------------------------------------------------------------
 * defineRecord
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t RecordObject::defineRecord(const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields)
{
    assert(rec_type);
    return addDefinition(NULL, rec_type, id_field, data_size, fields, num_fields, max_fields);
}

/*----------------------------------------------------------------------------
 * defineField
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t RecordObject::defineField(const char* rec_type, const char* field_name, fieldType_t type, int offset, int size, const char* exttype, unsigned int flags)
{
    assert(rec_type);
    assert(field_name);

    return addField(getDefinition(rec_type), field_name, type, offset, size, exttype, flags);
}

/*----------------------------------------------------------------------------
 * isRecord
 *----------------------------------------------------------------------------*/
bool RecordObject::isRecord(const char* rec_type)
{
    if(getDefinition(rec_type) != NULL) return true;
    else                                return false;
}

/*----------------------------------------------------------------------------
 * isType
 *----------------------------------------------------------------------------*/
bool RecordObject::isType(unsigned char* buffer, int size, const char* rec_type)
{
    const char* buf_type = NULL;
    if(parseSerial(buffer, size, &buf_type, NULL) > 0)
    {
        return (StringLib::match(rec_type, buf_type));
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getRecords
 *----------------------------------------------------------------------------*/
int RecordObject::getRecords(char*** rec_types)
{
    return definitions.getKeys(rec_types);
}

/*----------------------------------------------------------------------------
 * getRecordIdField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordIdField(const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->id_field;
}

/*----------------------------------------------------------------------------
 * getRecordSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordSize(const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return 0;
    return def->record_size;
}

/*----------------------------------------------------------------------------
 * getRecordDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordDataSize (const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return 0;
    return def->data_size;
}

/*----------------------------------------------------------------------------
 * getRecordMaxFields
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordMaxFields(const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return 0;
    return def->fields.getHashSize();
}

/*----------------------------------------------------------------------------
 * getRecordFields
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordFields(const char* rec_type, char*** field_names, field_t*** fields)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return 0;

    int num_fields = def->fields.getKeys(field_names);
    if(num_fields > 0)
    {
        *fields = new field_t* [num_fields];
        for(int i = 0; i < num_fields; i++)
        {
            (*fields)[i] = new field_t;
            try
            {
                *(*fields)[i] = def->fields[(*field_names)[i]];
            }
            catch(RunTimeException& e)
            {
                (void)e;
                (*fields)[i]->type = INVALID_FIELD;
            }
        }
    }
    return num_fields;
}

/*----------------------------------------------------------------------------
 * parseSerial
 *
 *  Allocates no memory, returns size of type
 *----------------------------------------------------------------------------*/
int RecordObject::parseSerial(unsigned char* buffer, int size, const char** rec_type, const unsigned char** rec_data)
{
    if(rec_type) *rec_type = NULL;
    if(rec_data) *rec_data = NULL;

    for(int i = sizeof(rec_hdr_t); i < size; i++)
    {
        if(buffer[i] == '\0')
        {
            if(rec_type) *rec_type = (const char*)buffer;
            if(i < size - 1)
            {
                if(rec_data) *rec_data = (const unsigned char*)&buffer[i+1];
            }
            return i + 1;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * postSerial
 *----------------------------------------------------------------------------*/
int RecordObject::postSerial (Publisher* outq, int timeout, const char* rec_type, int rec_type_size, unsigned char* buffer, int size)
{
    const int MAX_REC_TYPE_SIZE = 128;
    char data1[MAX_REC_TYPE_SIZE];

    int data1_size = sizeof(rec_hdr_t) + rec_type_size;
    if(data1_size > MAX_REC_TYPE_SIZE) return -1;

    populateHeader(data1, rec_type, rec_type_size, size);

    return outq->postCopy(data1, data1_size, buffer, size, timeout);
}

/*----------------------------------------------------------------------------
 * str2flags
 *----------------------------------------------------------------------------*/
unsigned int RecordObject::str2flags (const char* str)
{
    unsigned int flags = NATIVE_FLAGS;
    SafeString flagss("%s", str);
    List<SafeString>* flaglist = flagss.split('|');
    for(int i = 0; i < flaglist->length(); i++)
    {
        const char* flag = (*flaglist)[i].getString(false);
        if(StringLib::match(flag, "NATIVE"))    flags = NATIVE_FLAGS;
        else if(StringLib::match(flag, "LE"))   flags &= ~BIGENDIAN;
        else if(StringLib::match(flag, "BE"))   flags |= BIGENDIAN;
        else if(StringLib::match(flag, "PTR"))  flags |= POINTER;
    }
    delete flaglist;
    return flags;
}

/*----------------------------------------------------------------------------
 * flags2str
 *
 *  must free returned string
 *----------------------------------------------------------------------------*/
const char* RecordObject::flags2str (unsigned int flags)
{
    SafeString flagss;

    if(flags & BIGENDIAN)   flagss += "BE";
    else                    flagss += "LE";

    if(flags & POINTER)     flagss += "|PTR";

    return flagss.getString(true);
}

/*----------------------------------------------------------------------------
 * str2ft
 *----------------------------------------------------------------------------*/
RecordObject::fieldType_t RecordObject::str2ft (const char* str)
{
         if(StringLib::match(str, "INT8"))      return INT8;
    else if(StringLib::match(str, "INT16"))     return INT16;
    else if(StringLib::match(str, "INT32"))     return INT32;
    else if(StringLib::match(str, "INT64"))     return INT64;
    else if(StringLib::match(str, "UINT8"))     return UINT8;
    else if(StringLib::match(str, "UINT16"))    return UINT16;
    else if(StringLib::match(str, "UINT32"))    return UINT32;
    else if(StringLib::match(str, "UINT64"))    return UINT64;
    else if(StringLib::match(str, "BITFIELD"))  return BITFIELD;
    else if(StringLib::match(str, "FLOAT"))     return FLOAT;
    else if(StringLib::match(str, "DOUBLE"))    return DOUBLE;
    else if(StringLib::match(str, "TIME8"))     return TIME8;
    else if(StringLib::match(str, "STRING"))    return STRING;
    else if(StringLib::match(str, "USER"))      return USER;
    else if(StringLib::match(str, "INT16BE"))   return INT16;
    else if(StringLib::match(str, "INT32BE"))   return INT32;
    else if(StringLib::match(str, "INT64BE"))   return INT64;
    else if(StringLib::match(str, "UINT16BE"))  return UINT16;
    else if(StringLib::match(str, "UINT32BE"))  return UINT32;
    else if(StringLib::match(str, "UINT64BE"))  return UINT64;
    else if(StringLib::match(str, "FLOATBE"))   return FLOAT;
    else if(StringLib::match(str, "DOUBLEBE"))  return DOUBLE;
    else if(StringLib::match(str, "TIME8BE"))   return TIME8;
    else if(StringLib::match(str, "INT16LE"))   return INT16;
    else if(StringLib::match(str, "INT32LE"))   return INT32;
    else if(StringLib::match(str, "INT64LE"))   return INT64;
    else if(StringLib::match(str, "UINT16LE"))  return UINT16;
    else if(StringLib::match(str, "UINT32LE"))  return UINT32;
    else if(StringLib::match(str, "UINT64LE"))  return UINT64;
    else if(StringLib::match(str, "FLOATLE"))   return FLOAT;
    else if(StringLib::match(str, "DOUBLELE"))  return DOUBLE;
    else if(StringLib::match(str, "TIME8LE"))   return TIME8;
    else                                        return INVALID_FIELD;
}

/*----------------------------------------------------------------------------
 * str2be
 *----------------------------------------------------------------------------*/
bool RecordObject::str2be (const char* str)
{
    #define _IS_BIGENDIAN ((NATIVE_FLAGS & BIGENDIAN) == BIGENDIAN)

         if(StringLib::match(str, "BE"))        return true;
    else if(StringLib::match(str, "LE"))        return false;
    else if(StringLib::match(str, "INT8"))      return _IS_BIGENDIAN;
    else if(StringLib::match(str, "INT16"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "INT32"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "INT64"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "UINT8"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "UINT16"))    return _IS_BIGENDIAN;
    else if(StringLib::match(str, "UINT32"))    return _IS_BIGENDIAN;
    else if(StringLib::match(str, "UINT64"))    return _IS_BIGENDIAN;
    else if(StringLib::match(str, "BITFIELD"))  return _IS_BIGENDIAN;
    else if(StringLib::match(str, "FLOAT"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "DOUBLE"))    return _IS_BIGENDIAN;
    else if(StringLib::match(str, "TIME8"))     return _IS_BIGENDIAN;
    else if(StringLib::match(str, "STRING"))    return _IS_BIGENDIAN;
    else if(StringLib::match(str, "INT16BE"))   return true;
    else if(StringLib::match(str, "INT32BE"))   return true;
    else if(StringLib::match(str, "INT64BE"))   return true;
    else if(StringLib::match(str, "UINT16BE"))  return true;
    else if(StringLib::match(str, "UINT32BE"))  return true;
    else if(StringLib::match(str, "UINT64BE"))  return true;
    else if(StringLib::match(str, "FLOATBE"))   return true;
    else if(StringLib::match(str, "DOUBLEBE"))  return true;
    else if(StringLib::match(str, "TIME8BE"))   return true;
    else if(StringLib::match(str, "INT16LE"))   return false;
    else if(StringLib::match(str, "INT32LE"))   return false;
    else if(StringLib::match(str, "INT64LE"))   return false;
    else if(StringLib::match(str, "UINT16LE"))  return false;
    else if(StringLib::match(str, "UINT32LE"))  return false;
    else if(StringLib::match(str, "UINT64LE"))  return false;
    else if(StringLib::match(str, "FLOATLE"))   return false;
    else if(StringLib::match(str, "DOUBLELE"))  return false;
    else if(StringLib::match(str, "TIME8LE"))   return false;
    else                                        return _IS_BIGENDIAN; // default native
}

/*----------------------------------------------------------------------------
 * ft2str
 *----------------------------------------------------------------------------*/
const char* RecordObject::ft2str (fieldType_t ft)
{
    switch(ft)
    {
        case INT8:      return "INT8";
        case INT16:     return "INT16";
        case INT32:     return "INT32";
        case INT64:     return "INT64";
        case UINT8:     return "UINT8";
        case UINT16:    return "UINT16";
        case UINT32:    return "UINT32";
        case UINT64:    return "UINT64";
        case BITFIELD:  return "BITFIELD";
        case FLOAT:     return "FLOAT";
        case DOUBLE:    return "DOUBLE";
        case TIME8:     return "TIME8";
        case STRING:    return "STRING";
        case USER:      return "USER";
        default:        return "INVALID_FIELD";
    }
}

/*----------------------------------------------------------------------------
 * vt2str
 *----------------------------------------------------------------------------*/
const char* RecordObject::vt2str (valType_t vt)
{
    switch(vt)
    {
        case TEXT:      return "TEXT";
        case REAL:      return "REAL";
        case INTEGER:   return "INTEGER";
        default:        return "DYNAMIC";
    }
}

/*----------------------------------------------------------------------------
 * unpackBitField
 *
 *  TODO only supports big endian representation
 *----------------------------------------------------------------------------*/
unsigned long RecordObject::unpackBitField (unsigned char* buf, int bit_offset, int bit_length)
{
    /* Setup Parameters */
    int bits_left = bit_length;
    int bits_to_lsb = bit_length + bit_offset;
    int initial_shift = (sizeof(long) - bits_to_lsb) % 8;
    int byte_index = TOBYTES(bits_to_lsb - 1);

    /* Decompose Packet */
    int i = 0;
    unsigned long _value = 0;
    while(bits_left > 0)
    {
        unsigned int contribution = buf[byte_index--] >> initial_shift;
        unsigned int byte_mask = (1 << MIN(8, bits_left)) - 1;
        _value += (contribution & byte_mask) * (1L << (i++ * 8));
        bits_left -= 8;
        initial_shift = 0;
    }

    return _value;
}

/*----------------------------------------------------------------------------
 * packBitField
 *
 *  TODO only supports big endian representation
 *----------------------------------------------------------------------------*/
void RecordObject::packBitField (unsigned char* buf, int bit_offset, int bit_length, long val)
{
    int bits_remaining = bit_length;
    int bytes_remaining = TOBYTES(bit_length);
    int base_size_in_bits = TOBYTES(bit_length + 7);
    while(bits_remaining > 0)
    {
        int out_byte = (bit_offset + base_size_in_bits + bits_remaining) / 8;
        int bits_in_out_byte = (bit_offset + base_size_in_bits + bits_remaining) % 8;

        if(bits_in_out_byte == 0)
        {
            bits_in_out_byte = 8;
            out_byte--;
        }

        buf[out_byte] |= ((val >> ((TOBYTES(bit_length) - bytes_remaining) * 8)) << (8 - bits_in_out_byte)) & 0xFF;;

        bits_remaining -= bits_in_out_byte;
        bytes_remaining -= 1;
    }
}

/*----------------------------------------------------------------------------
 * parseImmediateField
 *
 *  Format: #<type>(<offset>,<size>)
 *
 *  Constraints: there are no spaces, and offset and size are in bits
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::parseImmediateField(const char* str)
{
    field_t retfield = {INVALID_FIELD, 0, 0, NULL, 0};
    field_t f = {INVALID_FIELD, 0, 0, NULL, 0};

    /* Make Copy of String */
    char pstr[MAX_VAL_STR_SIZE];
    StringLib::copy(pstr, str, MAX_VAL_STR_SIZE);

    /* Check Immediate Symbol */
    if(pstr[0] != IMMEDIATE_FIELD_SYMBOL) return retfield;

    /* Check Format */
    char* div = StringLib::find(&pstr[1], '(');
    if(div == NULL)
    {
        mlog(CRITICAL, "Missing leading parenthesis in %s", str);
        return retfield;
    }

    /* Set Type */
    *div = '\0';
    char* type_str = &pstr[1];
    fieldType_t type = str2ft(type_str);
    if(type == INVALID_FIELD)
    {
        mlog(CRITICAL, "Invalid field type: %s", type_str);
        return retfield;
    }
    f.type = type;

    /* Set Big Endian */
    if(str2be(type_str))    f.flags |= BIGENDIAN;
    else                    f.flags &= ~BIGENDIAN;

    /* Set Offset */
    char* offset_str = div + 1;
    div = StringLib::find(offset_str, ',');
    if(div == NULL)
    {
        mlog(CRITICAL, "Missing first comma in %s", str);
        return retfield;
    }
    *div = '\0';
    long offset;
    if(StringLib::str2long(offset_str, &offset))
    {
        f.offset = offset;
    }
    else
    {
        mlog(CRITICAL, "Invalid offset: %s", offset_str);
        return retfield;
    }

    /* Set Size */
    char* size_str = div + 1;
    div = StringLib::find(size_str, ',');
    if(div == NULL)
    {
        mlog(CRITICAL, "Missing second comma in %s", str);
        return retfield;
    }
    *div = '\0';
    long elements;
    if(StringLib::str2long(size_str, &elements))
    {
        f.elements = elements;
    }
    else
    {
        mlog(CRITICAL, "Invalid size: %s", size_str);
        return retfield;
    }

    /* Check Format */
    div += 2;
    if(*div != ')')
    {
        mlog(CRITICAL, "Missing trailing parenthesis in %s (%c)", str, *div);
        return retfield;
    }

    /* Return Actual Field */
    return f;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RecordObject::RecordObject(void)
{
    recordDefinition = NULL;
    recordMemory = NULL;
    recordData = NULL;
    memoryAllocated = 0;
    memoryOwner = false;
}

/*----------------------------------------------------------------------------
 * addDefinition
 *
 *  returns pointer to record definition in rec_def
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::getPointedToField(field_t f, bool allow_null, int element)
{
    if(f.flags & POINTER)
    {
        // Build Pointer Field
        field_t ptr_field = f;
        ptr_field.flags &= ~POINTER; // clear pointer flag
        ptr_field.type = INT32;

        // Update Field
        f.flags &= ~POINTER;
        f.offset = (int)getValueInteger(ptr_field, element);
        if(f.type != BITFIELD) f.offset *= 8; // bit fields are specified in bits from the start

        // Handle String Pointers
        if(f.type == STRING)
        {
            // string cannot extend past end of record
            f.elements = memoryAllocated - recordDefinition->type_size - TOBYTES(f.offset);
        }

        // Check Offset
        if(f.offset == 0 && !allow_null)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Attempted to dereference null pointer field!");
        }
        else if(f.offset > ((memoryAllocated - recordDefinition->type_size) * 8))
        {
            // Note that this check is only performed when memory has been allocated
            // this means that for a RecordInterface access to the record memory goes unchecked
            throw RunTimeException(CRITICAL, RTE_ERROR, "Pointer access exceeded size of memory allocated!");
        }
    }

    return f;
}

/*----------------------------------------------------------------------------
 * getUserField
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::getUserField (definition_t* def, const char* field_name)
{
    assert(field_name);

    field_t field = { INVALID_FIELD, 0, 0, NULL, NATIVE_FLAGS };
    long element = -1;

    /* Attempt Direct Access */
    try
    {
        field = def->fields[field_name];
    }
    catch(const RunTimeException& e)
    {
        (void)e;
    }

    /* Attempt Indirect Access (array and/or struct) */
    if(field.type == INVALID_FIELD) try
    {
        /* Make Mutable Copy of Field Name */
        char fstr[MAX_VAL_STR_SIZE]; // field string
        StringLib::copy(fstr, field_name, MAX_VAL_STR_SIZE);

        /* Parse Dotted Notation
        *      field.subfield
        *      ^     ^
        *      |     |
        *     fstr  subfield..
        */
        char* subfield_name = StringLib::find(&fstr[1], '.'); // user structure
        if(subfield_name)
        {
            *subfield_name = '\0';
            subfield_name += 1;
        }

        /* Parse Bracket Notation:
        *      field[element]
        *      ^     ^      ^
        *      |     |      |
        *     fstr  ele..  tstr
        */
        char* element_str = StringLib::find(&fstr[1], '['); // element string
        if(element_str != NULL)
        {
            char* tstr = StringLib::find(element_str, ']');
            if(tstr != NULL)
            {
                *element_str = '\0'; // terminate field name
                *tstr = '\0'; // terminate element string
                element_str += 1; // point to start of element string

                /* Get Element */
                if(!StringLib::str2long(element_str, &element))
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid array element!");
                }
            }
        }

        /* Look Up Field Name */
        field = def->fields[fstr];
        if(field.type != USER)
        {
            /* Check Element Boundary */
            if(element >= 0 && (element < field.elements || field.elements <= 0))
            {
                /* Modify Elements and Offset if not Pointer */
                if((field.flags & POINTER) == 0)
                {
                    if(field.elements > 0) field.elements -= element;
                    field.offset += TOBITS(element * FIELD_TYPE_BYTES[field.type]);
                }
            }
        }
        else
        {
            definition_t* subdef = definitions[field.exttype];
            field_t subfield = getUserField(subdef, subfield_name);
            subfield.offset += field.offset;
            field = subfield;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to parse field %s: %s", field_name, e.what());
    }

    /* Return Field */
    return field;
}

/*----------------------------------------------------------------------------
 * addDefinition
 *
 *  returns pointer to record definition in rec_def
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t RecordObject::addDefinition(definition_t** rec_def, const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields)
{
    recordDefErr_t status = SUCCESS_DEF;
    definition_t* def = NULL;

    /* Check Maximum Fields */
    if(max_fields == CALC_MAX_FIELDS)
    {
        max_fields = (int)(num_fields * 1.5);
    }
    else if(num_fields > max_fields)
    {
        return NUMFIELDERR_DEF;
    }

    defMut.lock();
    {
        /* Add Definition */
        def = getDefinition(rec_type);
        if(def == NULL)
        {
            assert(data_size > 0);
            def = new definition_t(rec_type, id_field, data_size, max_fields);
            if(!definitions.add(rec_type, def))
            {
                delete def;
                def = NULL;
                status = REGERR_DEF;
            }
        }
        else
        {
            status = DUPLICATE_DEF;
        }
    }
    defMut.unlock();

    /* Add Fields */
    for(int i = 0; status == SUCCESS_DEF && i < num_fields; i++)
    {
        status = addField(def, fields[i].name, fields[i].type, fields[i].offset, fields[i].elements, fields[i].exttype, fields[i].flags);
    }

    /* Return Definition and Status */
    if(rec_def) *rec_def = def;
    return status;
}

/*----------------------------------------------------------------------------
 * addField
 *
 *  Notes:
 *   1. Will often fail with multiple calls to create same record definition (this is okay)
 *   2. Offset in bytes except for bit field, where it is in bits
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t RecordObject::addField(definition_t* def, const char* field_name, fieldType_t type, int offset, int elements, const char* exttype, unsigned int flags)
{
    assert(field_name);

    /* Check Definition */
    if(!def) return NOTFOUND_DEF;
    else if(!field_name) return FIELDERR_DEF;

    /* Initialize Parameters */
    recordDefErr_t status = FIELDERR_DEF;
    int end_of_field;
    if(flags & POINTER) end_of_field = offset + FIELD_TYPE_BYTES[INT32];
    else                end_of_field = type == BITFIELD ? TOBYTES(offset + elements) : offset + (elements * FIELD_TYPE_BYTES[type]);
    int field_offset = type == BITFIELD ? offset : TOBITS(offset);

    /* Define Field */
    if(end_of_field <= def->data_size)
    {
        field_t f;
        f.type = type;
        f.offset = field_offset;
        f.elements = elements;
        f.exttype = exttype;
        f.flags = flags;

        // uniquely add the field
        if(def->fields.add(field_name, f, true))
        {
            status = SUCCESS_DEF;
        }
        else
        {
            status = DUPLICATE_DEF;
        }
    }

    /* Return Field and Status */
    return status;
}
/*----------------------------------------------------------------------------
 * populateHeader
 *----------------------------------------------------------------------------*/
void* RecordObject::populateHeader (char* buf, const char* type_name, int type_size, int data_size)
{
    #ifdef __be__
    rec_hdr_t hdr = {
        .version = RECORD_FORMAT_VERSION,
        .rectype_length = def->type_size,
        .data_length = data_size
    };
    #else
        rec_hdr_t hdr = {
            .version = LocalLib::swaps(RECORD_FORMAT_VERSION),
            .type_size = LocalLib::swaps(type_size),
            .data_size = LocalLib::swaps(data_size)
        };
    #endif

    LocalLib::copy(buf, &hdr, sizeof(rec_hdr_t));
    LocalLib::copy(&buf[sizeof(rec_hdr_t)], type_name, type_size);

    return &buf[sizeof(rec_hdr_t) + type_size];
}

/*----------------------------------------------------------------------------
 * getDefinition
 *----------------------------------------------------------------------------*/
RecordObject::definition_t* RecordObject::getDefinition(const char* rec_type)
{
    definition_t* def = NULL;
    try { def = definitions[rec_type]; }
    catch (RunTimeException& e) { (void)e; }
    return def;
}

/*----------------------------------------------------------------------------
 * getDefinition
 *
 *  Notes:
 *   1. operates on serialized data (not a population string)
 *----------------------------------------------------------------------------*/
RecordObject::definition_t* RecordObject::getDefinition(unsigned char* buffer, int size)
{
    /* Check Parameters */
    if(buffer == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Null buffer used to retrieve record definition");
    else if(size <= (int)sizeof(rec_hdr_t)) throw RunTimeException(CRITICAL, RTE_ERROR, "Buffer too small to retrieve record definition");

    /* Get Record Definitions */
    char* rec_type = (char*)&buffer[sizeof(rec_hdr_t)];
    definition_t* def = getDefinition(rec_type);

    /* Check Record Definition */
    // this doesn't throw an exception
    // because it was a well formed record
    if(def == NULL) return NULL;

    /* Return Definition*/
    return def;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RecordInterface::RecordInterface(unsigned char* buffer, int size): RecordObject()
{
    recordDefinition = getDefinition(buffer, size);
    if(recordDefinition != NULL)
    {
        if (size >= recordDefinition->record_size)
        {
            if(parseSerial(buffer, size, (const char**)&recordMemory, (const unsigned char**)&recordData) > 0)
            {
                memoryOwner = false;
                memoryAllocated = size;
            }
            else throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to differentiate the record type from record data");
        }
        else throw RunTimeException(CRITICAL, RTE_ERROR, "Buffer passed in not large enough to populate record");
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Could not find a definition that matches the record buffer");
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RecordInterface::~RecordInterface(void)
{

}
