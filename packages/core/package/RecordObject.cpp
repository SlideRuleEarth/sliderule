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

Dictionary<RecordObject::definition_t*> RecordObject::definitions;
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
    0, // INVALID_FIELD
    1  // BOOL
};

/******************************************************************************
 * RECORD OBJECT FIELD METHODS
 ******************************************************************************/

// NOLINTBEGIN(misc-no-recursion)

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
RecordObject::Field::Field(RecordObject& _rec, const field_t& _field, int _element):
    record(_rec),
    field(_field),
    element(_element)
{
}

/*----------------------------------------------------------------------------
 * Constructor (COPY)
 *----------------------------------------------------------------------------*/
RecordObject::Field::Field(const Field& f) = default;

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
    return RecordObject::getValueType(field);
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Attempts to create record from a string specification
 *----------------------------------------------------------------------------*/
RecordObject::RecordObject(const char* rec_type, int allocated_memory, bool clear)
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
            memoryUsed = memoryAllocated;
            data_size = recordDefinition->data_size;
        }
        else if(allocated_memory + (int)sizeof(rec_hdr_t) + recordDefinition->type_size >= recordDefinition->record_size)
        {
            memoryAllocated = allocated_memory + sizeof(rec_hdr_t) + recordDefinition->type_size;
            memoryUsed = memoryAllocated;
            data_size = allocated_memory;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid memory allocation in record creation for <%s>: %d + %d + %d < %d", rec_type, allocated_memory, (int)sizeof(rec_hdr_t), recordDefinition->type_size, recordDefinition->record_size);
        }

        /* Allocate Record Memory */
        memoryOwner = true;
        recordMemory = new unsigned char[memoryAllocated];

        /* Populate Header */
        rec_hdr_t hdr = {
            .version = OsApi::swaps(RECORD_FORMAT_VERSION),
            .type_size = OsApi::swaps(recordDefinition->type_size),
            .data_size = OsApi::swapl(data_size)
        };
        memcpy(recordMemory, &hdr, sizeof(rec_hdr_t));
        memcpy(&recordMemory[sizeof(rec_hdr_t)], recordDefinition->type_name, recordDefinition->type_size);

        /* Set Record Data Pointer */
        recordData = &recordMemory[sizeof(rec_hdr_t) + recordDefinition->type_size];

        /* Zero Out Record Data */
        if(clear) memset(recordData, 0, recordDefinition->data_size);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "could not locate record definition %s", rec_type);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Assumes a serialized buffer <type string><binary data>
 *  (another way to perform deserialization)
 *----------------------------------------------------------------------------*/
RecordObject::RecordObject(const unsigned char* buffer, int size)
{
    recordDefinition = getDefinition(buffer, size);
    if(recordDefinition != NULL)
    {
        if (size >= recordDefinition->record_size)
        {
            /* Set Record Memory */
            memoryOwner = true;
            memoryAllocated = size;
            memoryUsed = memoryAllocated;
            recordMemory = new unsigned char[memoryAllocated];
            memcpy(recordMemory, buffer, memoryAllocated);

            /* Set Record Data */
            recordData = &recordMemory[sizeof(rec_hdr_t) + recordDefinition->type_size];
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "buffer passed in not large enough to populate record");
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "buffer did not contain defined record");
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
    const definition_t* def = getDefinition(buffer, size);
    if(def != recordDefinition)
    {
        return false; // record does not match definition
    }

    /* Check Size */
    if(size > memoryUsed)
    {
        return false; // buffer passed in too large
    }

    if(size < def->type_size)
    {
        return false; // buffer not large enough to populate type string
    }

    /* Copy Data and Return */
    memcpy(recordMemory, buffer, size);
    return true;
}

/*----------------------------------------------------------------------------
 * serialize
 *----------------------------------------------------------------------------*/
int RecordObject::serialize(unsigned char** buffer, serialMode_t mode, int size)
{
    assert(buffer);
    int bufsize = memoryUsed;
    uint32_t datasize = 0;

    /* Determine Buffer Size */
    const rec_hdr_t* rechdr = reinterpret_cast<const rec_hdr_t*>(recordMemory);
    if(size > 0)
    {
        const int hdrsize = sizeof(rec_hdr_t) + OsApi::swaps(rechdr->type_size);
        datasize = size;
        bufsize = hdrsize + datasize;
    }

    /* Allocate or Copy Buffer */
    if (mode == ALLOCATE)
    {
        *buffer = new unsigned char[bufsize];
        const int bytes_to_copy = MIN(bufsize, memoryUsed);
        memcpy(*buffer, recordMemory, bytes_to_copy);
    }
    else if (mode == REFERENCE)
    {
        *buffer = recordMemory;
    }
    else if (mode == TAKE_OWNERSHIP)
    {
        *buffer = recordMemory;
        memoryOwner = false;
    }
    else // if (mode == COPY)
    {
        assert(*buffer);
        const int bytes_to_copy = MIN(bufsize, memoryUsed);
        memcpy(*buffer, recordMemory, bytes_to_copy);
    }

    /* Set Size in Record Header */
    if(size > 0)
    {
        rec_hdr_t* bufhdr = reinterpret_cast<rec_hdr_t*>(*buffer);
        bufhdr->data_size = OsApi::swapl(datasize);
    }

    /* Return Buffer Size */
    return bufsize;
}

/*----------------------------------------------------------------------------
 * post
 *  - by default this can only be called once for a record because the serial
 *    mode is set to TAKE_OWNERSHIP which means the record memory is given
 *    to the message queue and freed by the queue when it is dereferenced
 *  - to call this function multiple times for a given record, the mode must
 *    be set to ALLOCATE
 *----------------------------------------------------------------------------*/
template<typename Flag>
bool RecordObject::post(Publisher* outq, int size, const Flag* active, bool verbose, int timeout, serialMode_t mode)
{
    bool status = true;

    /* Serialize Record */
    uint8_t* rec_buf = NULL;
    const int rec_bytes = serialize(&rec_buf, mode, size);

    /* Post Record — use FlagOps to load bool vs atomic */
    int post_status = MsgQ::STATE_TIMEOUT;

    while (EventLib::FlagOps<Flag>::load(active) &&
           ((post_status = outq->postRef(rec_buf, rec_bytes, timeout)) == MsgQ::STATE_TIMEOUT));

    /* Handle Status */
    if(post_status <= 0)
    {
        delete [] rec_buf; // we've taken ownership
        if(verbose) mlog(ERROR, "Failed to post %s to stream %s: %d", getRecordType(), outq->getName(), post_status);
        status = false;
    }

    return status;
}

bool RecordObject::post(Publisher* outq, int size, std::nullptr_t, bool verbose, int timeout, serialMode_t mode)
{
    /* Treat NULL exactly like "no active flag" */
    const bool* active = nullptr;
    return post<bool>(outq, size, active, verbose, timeout, mode);
}

/* Force generation of required template instantiations for RecordObject::post<> */
template bool RecordObject::post<bool>(Publisher*, int, const bool*, bool, int, serialMode_t);
template bool RecordObject::post<std::atomic<bool>>(Publisher*, int, const std::atomic<bool>*, bool, int, serialMode_t);

/*----------------------------------------------------------------------------
 * isRecordType
 *----------------------------------------------------------------------------*/
bool RecordObject::isRecordType(const char* rec_type) const
{
    return (StringLib::match(rec_type, recordDefinition->type_name));
}

/*----------------------------------------------------------------------------
 * getRecordType
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordType(void) const
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
        const field_t f = getField(recordDefinition->id_field);

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
unsigned char* RecordObject::getRecordData(void) const
{
    return recordData;
}

/*----------------------------------------------------------------------------
 * getRecordTypeSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordTypeSize(void) const
{
    return recordDefinition->type_size;
}

/*----------------------------------------------------------------------------
 * getRecordDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordDataSize(void) const
{
    return recordDefinition->data_size;
}

/*----------------------------------------------------------------------------
 * getAllocatedMemory
 *----------------------------------------------------------------------------*/
int RecordObject::getAllocatedMemory(void) const
{
    return memoryAllocated;
}

/*----------------------------------------------------------------------------
 * getAllocatedDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getAllocatedDataSize(void) const
{
    return memoryAllocated - (sizeof(rec_hdr_t) + recordDefinition->type_size);
}

/*----------------------------------------------------------------------------
 * getUsedMemory
 *----------------------------------------------------------------------------*/
int RecordObject::getUsedMemory (void) const
{
    return memoryUsed;
}

/*----------------------------------------------------------------------------
 * getUsedDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getUsedDataSize (void) const
{
    return memoryUsed - (sizeof(rec_hdr_t) + recordDefinition->type_size);
}

/*----------------------------------------------------------------------------
 * createRecordField
 *----------------------------------------------------------------------------*/
RecordObject::Field* RecordObject::createRecordField(const char* field_name)
{
    RecordField* rec_field = NULL;
    const field_t f = getField(field_name);
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

    const int numtoks = StringLib::tokenizeLine(populate_string, StringLib::nsize(populate_string, MAX_STR_SIZE - 1) + 1, ' ', MAX_INITIALIZERS, toks);

    for(int i = 0; i < numtoks; i++)
    {
        char args[2][MAX_STR_SIZE];
        if(StringLib::tokenizeLine(toks[i], MAX_STR_SIZE, '=', 2, args) == 2)
        {
            const char* field_str = args[0];
            const char* value_str = args[1];

            const field_t f = getField(field_str);
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
        delete [] recordDefinition->id_field;
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
    return getUserField(recordDefinition, field_name);
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
void RecordObject::setValueText(const field_t& f, const char* val, int element)
{
    const valType_t val_type = getValueType(f);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, false, element);
        if(val == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Cannot null existing pointer!");
        setValueText(ptr_field, val);
    }
    else if(val_type == TEXT)
    {
        const int val_len = StringLib::nsize(val, MAX_VAL_STR_SIZE) + 1;
        if(val_len <= f.elements)
        {
            memcpy(recordData + TOBYTES(f.offset), reinterpret_cast<const unsigned char*>(val), val_len);
        }
        else if(f.elements > 0)
        {
            memcpy(recordData + TOBYTES(f.offset), reinterpret_cast<const unsigned char*>(val), f.elements - 1);
            *(recordData + TOBYTES(f.offset) + f.elements - 1) = '\0';
        }
        else // variable length
        {
            const int memory_left = MIN(MAX_VAL_STR_SIZE, memoryAllocated - recordDefinition->type_size - TOBYTES(f.offset));
            if(memory_left > 1)
            {
                memcpy(recordData + TOBYTES(f.offset), reinterpret_cast<const unsigned char*>(val), memory_left - 1);
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
void RecordObject::setValueReal(const field_t& f, double val, int element)
{
    if(f.elements > 0 && element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_FAILURE, "Out of range access");
    const uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);
    type_cast_t* cast = reinterpret_cast<type_cast_t*>(recordData + elem_offset);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, false, element);
        setValueReal(ptr_field, val, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN)) // architectures match
    {
        switch(f.type)
        {
            case INT8:      cast->int8_val   = static_cast<int8_t>(val);     break;
            case INT16:     cast->int16_val  = static_cast<int16_t>(val);    break;
            case INT32:     cast->int32_val  = static_cast<int32_t>(val);    break;
            case INT64:     cast->int64_val  = static_cast<int64_t>(val);    break;
            case UINT8:     cast->uint8_val  = static_cast<uint8_t>(val);    break;
            case UINT16:    cast->uint16_val = static_cast<uint16_t>(val);   break;
            case UINT32:    cast->uint32_val = static_cast<uint32_t>(val);   break;
            case UINT64:    cast->uint64_val = static_cast<uint64_t>(val);   break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, static_cast<long>(val));  break;
            case FLOAT:     cast->float_val  = static_cast<float>(val);      break;
            case DOUBLE:    cast->double_val = val;                          break;
            case TIME8:     cast->int64_val  = static_cast<int64_t>(val);    break;
            case STRING:    StringLib::format(reinterpret_cast<char*>(recordData + elem_offset), f.elements, DEFAULT_DOUBLE_FORMAT, val);
                            break;
            default:        break;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      cast->int8_val   = static_cast<int8_t>(val);                 break;
            case INT16:     cast->int16_val  = OsApi::swaps(static_cast<int16_t>(val));  break;
            case INT32:     cast->int32_val  = OsApi::swapl(static_cast<int32_t>(val));  break;
            case INT64:     cast->int64_val  = OsApi::swapll(static_cast<int64_t>(val)); break;
            case UINT8:     cast->uint8_val  = static_cast<uint8_t>(val);                break;
            case UINT16:    cast->uint16_val = OsApi::swaps(static_cast<uint16_t>(val)); break;
            case UINT32:    cast->uint32_val = OsApi::swapl(static_cast<uint32_t>(val)); break;
            case UINT64:    cast->uint64_val = OsApi::swapll(static_cast<uint64_t>(val));break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, static_cast<long>(val)); break;
            case FLOAT:     cast->float_val  = OsApi::swapf(static_cast<float>(val));    break;
            case DOUBLE:    cast->double_val = OsApi::swapf(val);                        break;
            case TIME8 :    cast->int64_val  = OsApi::swapll(static_cast<uint64_t>(val));break;
            case STRING:    StringLib::format(reinterpret_cast<char*>(recordData + elem_offset), f.elements, DEFAULT_DOUBLE_FORMAT, val);
                            break;
            default:        break;
        }
    }
}

/*----------------------------------------------------------------------------
 * setValueInteger
 *----------------------------------------------------------------------------*/
void RecordObject::setValueInteger(const field_t& f, long val, int element)
{
    if(f.elements > 0 && element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_FAILURE, "Out of range access");
    const uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);
    type_cast_t* cast = reinterpret_cast<type_cast_t*>(recordData + elem_offset);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, false, element);
        setValueInteger(ptr_field, val, 0);
    }
    else if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      cast->int8_val   = static_cast<int8_t>(val);     break;
            case INT16:     cast->int16_val  = static_cast<int16_t>(val);    break;
            case INT32:     cast->int32_val  = static_cast<int32_t>(val);    break;
            case INT64:     cast->int64_val  = static_cast<int64_t>(val);    break;
            case UINT8:     cast->uint8_val  = static_cast<uint8_t>(val);    break;
            case UINT16:    cast->uint16_val = static_cast<uint16_t>(val);   break;
            case UINT32:    cast->uint32_val = static_cast<uint32_t>(val);   break;
            case UINT64:    cast->uint64_val = static_cast<uint64_t>(val);   break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, val); break;
            case FLOAT:     cast->float_val  = static_cast<float>(val);      break;
            case DOUBLE:    cast->double_val = val;                           break;
            case TIME8:     cast->int64_val  = static_cast<int64_t>(val);    break;
            case STRING:    StringLib::format(reinterpret_cast<char*>(recordData + elem_offset), f.elements, DEFAULT_LONG_FORMAT, val);
                            break;
            default:        break;
        }
    }
    else // Swap
    {
        switch(f.type)
        {
            case INT8:      cast->int8_val   = static_cast<int8_t>(val);                   break;
            case INT16:     cast->int16_val  = OsApi::swaps(static_cast<int16_t>(val));    break;
            case INT32:     cast->int32_val  = OsApi::swapl(static_cast<int32_t>(val));    break;
            case INT64:     cast->int64_val  = OsApi::swapll(static_cast<int64_t>(val));   break;
            case UINT8:     cast->uint8_val  = static_cast<uint8_t>(val);                  break;
            case UINT16:    cast->uint16_val = OsApi::swaps(static_cast<uint16_t>(val));   break;
            case UINT32:    cast->uint32_val = OsApi::swapl(static_cast<uint32_t>(val));   break;
            case UINT64:    cast->uint64_val = OsApi::swapll(static_cast<uint64_t>(val));  break;
            case BITFIELD:  packBitField(recordData, f.offset, f.elements, val); break;
            case FLOAT:     cast->float_val  = OsApi::swapf(static_cast<float>(val));      break;
            case DOUBLE:    cast->double_val = OsApi::swaplf(static_cast<double>(val));    break;
            case TIME8:     cast->int64_val  = OsApi::swapll(static_cast<int64_t>(val));   break;
            case STRING:    StringLib::format(reinterpret_cast<char*>(recordData + elem_offset), f.elements, DEFAULT_LONG_FORMAT, val);
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
const char* RecordObject::getValueText(const field_t& f, char* valbuf, int element)
{
    const valType_t val_type = getValueType(f);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, true, element);
        if(ptr_field.offset == 0) return NULL;
        return getValueText(ptr_field, valbuf);
    }

    if(val_type == TEXT)
    {
        char* str = reinterpret_cast<char*>(recordData + TOBYTES(f.offset));
        if(valbuf)
        {
            if(f.elements > 0)
            {
                return StringLib::copy(valbuf, str, f.elements);
            }

            // variable length
            const int memory_left = MIN(MAX_VAL_STR_SIZE, memoryAllocated - recordDefinition->type_size - TOBYTES(f.offset));
            if(memory_left > 1)
            {
                return StringLib::copy(valbuf, str, memory_left);
            }
        }

        // valbuf not supplied
        return str;
    }

    if(val_type == INTEGER && valbuf)
    {
        const long val = getValueInteger(f);
        return StringLib::format(valbuf, MAX_VAL_STR_SIZE, DEFAULT_LONG_FORMAT, val);
    }

    if(val_type == REAL && valbuf)
    {
        const double val = getValueReal(f);
        return StringLib::format(valbuf, MAX_VAL_STR_SIZE, DEFAULT_DOUBLE_FORMAT, val);
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * getValueReal
 *----------------------------------------------------------------------------*/
double RecordObject::getValueReal(const field_t& f, int element)
{
    if(f.elements > 0 && element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_FAILURE, "Out of range access");
    const uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);
    type_cast_t* cast = reinterpret_cast<type_cast_t*>(recordData + elem_offset);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, false, element);
        return getValueReal(ptr_field, 0);
    }

    if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      return static_cast<double>(cast->int8_val);
            case INT16:     return static_cast<double>(cast->int16_val);
            case INT32:     return static_cast<double>(cast->int32_val);
            case INT64:     return static_cast<double>(cast->int64_val);
            case UINT8:     return static_cast<double>(cast->uint8_val);
            case UINT16:    return static_cast<double>(cast->uint16_val);
            case UINT32:    return static_cast<double>(cast->uint32_val);
            case UINT64:    return static_cast<double>(cast->uint64_val);
            case BITFIELD:  return static_cast<double>(unpackBitField(recordData, f.offset, f.elements));
            case FLOAT:     return static_cast<double>(cast->float_val);
            case DOUBLE:    return cast->double_val;
            case TIME8:     return static_cast<double>(cast->int64_val);
            default:        return 0.0;
        }
    }

    // Swap
    switch(f.type)
    {
        case INT8:      return static_cast<double>(cast->int8_val);
        case INT16:     return static_cast<double>(OsApi::swaps(cast->int16_val));
        case INT32:     return static_cast<double>(OsApi::swapl(cast->int32_val));
        case INT64:     return static_cast<double>(OsApi::swapll(cast->int64_val));
        case UINT8:     return static_cast<double>(cast->uint8_val);
        case UINT16:    return static_cast<double>(OsApi::swaps(cast->uint16_val));
        case UINT32:    return static_cast<double>(OsApi::swapl(cast->uint32_val));
        case UINT64:    return static_cast<double>(OsApi::swapll(cast->uint64_val));
        case BITFIELD:  return static_cast<double>(unpackBitField(recordData, f.offset, f.elements));
        case FLOAT:     return static_cast<double>(OsApi::swapf(cast->float_val));
        case DOUBLE:    return OsApi::swaplf(cast->double_val);
        case TIME8:     return static_cast<double>(OsApi::swapll(cast->int64_val));
        default:        return 0.0;
    }
}

/*----------------------------------------------------------------------------
 * getValueInteger
 *----------------------------------------------------------------------------*/
long RecordObject::getValueInteger(const field_t& f, int element)
{
    if(f.elements > 0 && element > 0 && element >= f.elements) throw RunTimeException(CRITICAL, RTE_FAILURE, "Out of range access");
    const uint32_t elem_offset = TOBYTES(f.offset) + (element * FIELD_TYPE_BYTES[f.type]);
    type_cast_t* cast = reinterpret_cast<type_cast_t*>(recordData + elem_offset);

    if(f.flags & POINTER)
    {
        const field_t ptr_field = getPointedToField(f, false, element);
        return getValueInteger(ptr_field, 0);
    }

    // Native
    if(NATIVE_FLAGS == (f.flags & BIGENDIAN))
    {
        switch(f.type)
        {
            case INT8:      return static_cast<long>(cast->int8_val);
            case INT16:     return static_cast<long>(cast->int16_val);
            case INT32:     return static_cast<long>(cast->int32_val);
            case INT64:     return static_cast<long>(cast->int64_val);
            case UINT8:     return static_cast<long>(cast->uint8_val);
            case UINT16:    return static_cast<long>(cast->uint16_val);
            case UINT32:    return static_cast<long>(cast->uint32_val);
            case UINT64:    return static_cast<long>(cast->uint64_val);
            case BITFIELD:  return static_cast<long>(unpackBitField(recordData, f.offset, f.elements));
            case FLOAT:     return static_cast<long>(cast->float_val);
            case DOUBLE:    return static_cast<long>(cast->double_val);
            case TIME8:     return static_cast<long>(cast->int64_val);
            default:        return 0;
        }
    }

    // Swap
    switch(f.type)
    {
        case INT8:      return static_cast<long>(cast->int8_val);
        case INT16:     return static_cast<long>(OsApi::swaps(cast->int16_val));
        case INT32:     return static_cast<long>(OsApi::swapl(cast->int32_val));
        case INT64:     return static_cast<long>(OsApi::swapll(cast->int64_val));
        case UINT8:     return static_cast<long>(cast->uint8_val);
        case UINT16:    return static_cast<long>(OsApi::swaps(cast->uint16_val));
        case UINT32:    return static_cast<long>(OsApi::swapl(cast->uint32_val));
        case UINT64:    return static_cast<long>(OsApi::swapll(cast->uint64_val));
        case BITFIELD:  return static_cast<long>(unpackBitField(recordData, f.offset, f.elements));
        case FLOAT:     return static_cast<long>(OsApi::swapf(cast->float_val));
        case DOUBLE:    return static_cast<long>(OsApi::swaplf(cast->double_val));
        case TIME8:     return static_cast<long>(OsApi::swapll(cast->int64_val));
        default:        return 0;
    }
}

/*----------------------------------------------------------------------------
 * getValueType
 *----------------------------------------------------------------------------*/
bool RecordObject::setUsedData (int size)
{
    rec_hdr_t* rechdr = reinterpret_cast<rec_hdr_t*>(recordMemory);
    const int hdrsize = sizeof(rec_hdr_t) + OsApi::swaps(rechdr->type_size);
    const int bufsize = hdrsize + size;
    if(bufsize <= memoryAllocated)
    {
        memoryUsed = bufsize;
        rechdr->data_size = OsApi::swapl(size);
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getValueType
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::getDefinedField (const char* rec_type, const char* field_name)
{
    definition_t* def = getDefinition(rec_type);
    return getUserField(def, field_name);
}

/*----------------------------------------------------------------------------
 * getValueType
 *----------------------------------------------------------------------------*/
RecordObject::valType_t RecordObject::getValueType(const field_t& f)
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
        case TIME8:     return INTEGER;
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
    definition_t* rec_def = NULL;
    const recordDefErr_t status = addDefinition(&rec_def, rec_type, id_field, data_size, fields, num_fields, max_fields);
    if(status == SUCCESS_DEF) scanDefinition(rec_def, "", rec_type);
    return status;
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
    return (getDefinition(rec_type) != NULL);
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
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->id_field;
}

/*----------------------------------------------------------------------------
 * getRecordIndexField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordIndexField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.index_field;
}

/*----------------------------------------------------------------------------
 * getRecordTimeField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordTimeField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.time_field;
}

/*----------------------------------------------------------------------------
 * getRecordXField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordXField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.x_field;
}

/*----------------------------------------------------------------------------
 * getRecordYField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordYField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.y_field;
}

/*----------------------------------------------------------------------------
 * getRecordZField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordZField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.z_field;
}

/*----------------------------------------------------------------------------
 * getRecordBatchField
 *----------------------------------------------------------------------------*/
const char* RecordObject::getRecordBatchField (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return def->meta.batch_field;
}

/*----------------------------------------------------------------------------
 * getRecordMetaFields
 *----------------------------------------------------------------------------*/
RecordObject::meta_t* RecordObject::getRecordMetaFields (const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return &(def->meta);
}

/*----------------------------------------------------------------------------
 * getRecordSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordSize(const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
    if(def == NULL) return 0;
    return def->record_size;
}

/*----------------------------------------------------------------------------
 * getRecordDataSize
 *----------------------------------------------------------------------------*/
int RecordObject::getRecordDataSize (const char* rec_type)
{
    const definition_t* def = getDefinition(rec_type);
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

    const int num_fields = def->fields.getKeys(field_names);
    if(num_fields > 0)
    {
        *fields = new field_t* [num_fields];
        for(int i = 0; i < num_fields; i++)
        {
            (*fields)[i] = new field_t;
            try
            {
                *(*fields)[i] = def->fields[(*field_names)[i]];  // NOLINT(clang-analyzer-core.CallAndMessage)
            }
            catch(const RunTimeException& e)
            {
                (void)e;
                (*fields)[i]->type = INVALID_FIELD;
            }
        }
    }
    return num_fields;
}

/*----------------------------------------------------------------------------
 * getRecordFields
 *----------------------------------------------------------------------------*/
Dictionary<RecordObject::field_t>* RecordObject::getRecordFields (const char* rec_type)
{
    definition_t* def = getDefinition(rec_type);
    if(def == NULL) return NULL;
    return &def->fields;
}

/*----------------------------------------------------------------------------
 * parseSerial
 *
 *  Allocates no memory, returns size of type
 *----------------------------------------------------------------------------*/
int RecordObject::parseSerial(const unsigned char* buffer, int size, const char** rec_type, const unsigned char** rec_data)
{
    if(rec_type) *rec_type = NULL;
    if(rec_data) *rec_data = NULL;

    for(int i = sizeof(rec_hdr_t); i < size; i++)
    {
        if(buffer[i] == '\0')
        {
            if(rec_type) *rec_type = reinterpret_cast<const char*>(buffer);
            if(i < size - 1)
            {
                if(rec_data) *rec_data = &buffer[i+1];
            }
            return i + 1;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * str2flags
 *----------------------------------------------------------------------------*/
unsigned int RecordObject::str2flags (const char* str)
{
    unsigned int flags = NATIVE_FLAGS;
    List<string*>* flaglist = StringLib::split(str, strlen(str), '|');
    for(int i = 0; i < flaglist->length(); i++)
    {
        const char* flag = flaglist->get(i)->c_str();
        if(StringLib::match(flag, "NATIVE"))        flags = NATIVE_FLAGS;
        else if(StringLib::match(flag, "LE"))       flags &= ~BIGENDIAN;
        else if(StringLib::match(flag, "BE"))       flags |= BIGENDIAN;
        else if(StringLib::match(flag, "PTR"))      flags |= POINTER;
        else if(StringLib::match(flag, "AUX"))      flags |= AUX;
        else if(StringLib::match(flag, "BATCH"))    flags |= BATCH;
        else if(StringLib::match(flag, "X_COORD"))  flags |= X_COORD;
        else if(StringLib::match(flag, "Y_COORD"))  flags |= Y_COORD;
        else if(StringLib::match(flag, "TIME"))     flags |= TIME;
        else if(StringLib::match(flag, "INDEX"))    flags |= INDEX;
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
    string flagss;

    if(flags & BIGENDIAN)   flagss += "BE";
    else                    flagss += "LE";
    if(flags & POINTER)     flagss += "|PTR";
    if(flags & BATCH)       flagss += "|BATCH";
    if(flags & AUX)         flagss += "|AUX";
    if(flags & X_COORD)     flagss += "|X";
    if(flags & Y_COORD)     flagss += "|Y";
    if(flags & TIME)        flagss += "|T";
    if(flags & INDEX)       flagss += "|I";

    return StringLib::duplicate(flagss.c_str());
}

/*----------------------------------------------------------------------------
 * str2ft
 *----------------------------------------------------------------------------*/
RecordObject::fieldType_t RecordObject::str2ft (const char* str)
{
    if(StringLib::match(str, "INT8"))       return INT8;
    if(StringLib::match(str, "INT16"))      return INT16;
    if(StringLib::match(str, "INT32"))      return INT32;
    if(StringLib::match(str, "INT64"))      return INT64;
    if(StringLib::match(str, "UINT8"))      return UINT8;
    if(StringLib::match(str, "UINT16"))     return UINT16;
    if(StringLib::match(str, "UINT32"))     return UINT32;
    if(StringLib::match(str, "UINT64"))     return UINT64;
    if(StringLib::match(str, "BITFIELD"))   return BITFIELD;
    if(StringLib::match(str, "FLOAT"))      return FLOAT;
    if(StringLib::match(str, "DOUBLE"))     return DOUBLE;
    if(StringLib::match(str, "TIME8"))      return TIME8;
    if(StringLib::match(str, "STRING"))     return STRING;
    if(StringLib::match(str, "USER"))       return USER;
    if(StringLib::match(str, "INT16BE"))    return INT16;
    if(StringLib::match(str, "INT32BE"))    return INT32;
    if(StringLib::match(str, "INT64BE"))    return INT64;
    if(StringLib::match(str, "UINT16BE"))   return UINT16;
    if(StringLib::match(str, "UINT32BE"))   return UINT32;
    if(StringLib::match(str, "UINT64BE"))   return UINT64;
    if(StringLib::match(str, "FLOATBE"))    return FLOAT;
    if(StringLib::match(str, "DOUBLEBE"))   return DOUBLE;
    if(StringLib::match(str, "TIME8BE"))    return TIME8;
    if(StringLib::match(str, "INT16LE"))    return INT16;
    if(StringLib::match(str, "INT32LE"))    return INT32;
    if(StringLib::match(str, "INT64LE"))    return INT64;
    if(StringLib::match(str, "UINT16LE"))   return UINT16;
    if(StringLib::match(str, "UINT32LE"))   return UINT32;
    if(StringLib::match(str, "UINT64LE"))   return UINT64;
    if(StringLib::match(str, "FLOATLE"))    return FLOAT;
    if(StringLib::match(str, "DOUBLELE"))   return DOUBLE;
    if(StringLib::match(str, "TIME8LE"))    return TIME8;
    return INVALID_FIELD;
}

/*----------------------------------------------------------------------------
 * str2be
 *----------------------------------------------------------------------------*/
bool RecordObject::str2be (const char* str)
{
    #define _IS_BIGENDIAN ((NATIVE_FLAGS & BIGENDIAN) == BIGENDIAN)

    if(StringLib::match(str, "BE"))         return true;
    if(StringLib::match(str, "LE"))         return false;
    if(StringLib::match(str, "INT8"))       return _IS_BIGENDIAN;
    if(StringLib::match(str, "INT16"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "INT32"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "INT64"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "UINT8"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "UINT16"))     return _IS_BIGENDIAN;
    if(StringLib::match(str, "UINT32"))     return _IS_BIGENDIAN;
    if(StringLib::match(str, "UINT64"))     return _IS_BIGENDIAN;
    if(StringLib::match(str, "BITFIELD"))   return _IS_BIGENDIAN;
    if(StringLib::match(str, "FLOAT"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "DOUBLE"))     return _IS_BIGENDIAN;
    if(StringLib::match(str, "TIME8"))      return _IS_BIGENDIAN;
    if(StringLib::match(str, "STRING"))     return _IS_BIGENDIAN;
    if(StringLib::match(str, "INT16BE"))    return true;
    if(StringLib::match(str, "INT32BE"))    return true;
    if(StringLib::match(str, "INT64BE"))    return true;
    if(StringLib::match(str, "UINT16BE"))   return true;
    if(StringLib::match(str, "UINT32BE"))   return true;
    if(StringLib::match(str, "UINT64BE"))   return true;
    if(StringLib::match(str, "FLOATBE"))    return true;
    if(StringLib::match(str, "DOUBLEBE"))   return true;
    if(StringLib::match(str, "TIME8BE"))    return true;
    if(StringLib::match(str, "INT16LE"))    return false;
    if(StringLib::match(str, "INT32LE"))    return false;
    if(StringLib::match(str, "INT64LE"))    return false;
    if(StringLib::match(str, "UINT16LE"))   return false;
    if(StringLib::match(str, "UINT32LE"))   return false;
    if(StringLib::match(str, "UINT64LE"))   return false;
    if(StringLib::match(str, "FLOATLE"))    return false;
    if(StringLib::match(str, "DOUBLELE"))   return false;
    if(StringLib::match(str, "TIME8LE"))    return false;
    return _IS_BIGENDIAN; // default native
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
unsigned long RecordObject::unpackBitField (const unsigned char* buf, int bit_offset, int bit_length)
{
    /* Setup Parameters */
    int bits_left = bit_length;
    const int bits_to_lsb = bit_length + bit_offset;
    int initial_shift = (sizeof(long) - bits_to_lsb) % 8;
    int byte_index = TOBYTES(bits_to_lsb - 1);

    /* Decompose Packet */
    int i = 0;
    unsigned long _value = 0;
    while(bits_left > 0)
    {
        const unsigned int contribution = buf[byte_index--] >> initial_shift;
        const unsigned int byte_mask = (1 << MIN(8, bits_left)) - 1;
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
    const int base_size_in_bits = TOBYTES(bit_length + 7);
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
    const fieldType_t type = str2ft(type_str);
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
    memoryUsed = 0;
    memoryOwner = false;
}

/*----------------------------------------------------------------------------
 * getPointedToField
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
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Attempted to dereference null pointer field!");
        }
        if(f.offset > ((memoryAllocated - recordDefinition->type_size) * 8))
        {
            // Note that this check is only performed when memory has been allocated
            // this means that for a RecordInterface access to the record memory goes unchecked
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Pointer access exceeded size of memory allocated!");
        }
    }

    return f;
}

/*----------------------------------------------------------------------------
 * getUserField
 *----------------------------------------------------------------------------*/
RecordObject::field_t RecordObject::getUserField (definition_t* def, const char* field_name, uint32_t parent_flags)
{
    assert(field_name);

    field_t _field = { INVALID_FIELD, 0, 0, NULL, parent_flags };
    long element = -1;

    /* Sanity Check Def */
    if(def == NULL) return _field;

    /* Attempt Direct Access */
    try
    {
        _field = def->fields[field_name];
    }
    catch(const RunTimeException& e)
    {
        (void)e;
    }

    /* Attempt Indirect Access (array and/or struct) */
    if(_field.type == INVALID_FIELD) try
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
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid array element!");
                }
            }
        }

        /* Look Up Field Name */
        _field = def->fields[fstr];
        if(_field.type != USER)
        {
            /* Check Element Boundary */
            if(element >= 0 && (element < _field.elements || _field.elements <= 0))
            {
                /* Modify Elements and Offset if not Pointer */
                if((_field.flags & POINTER) == 0)
                {
                    if(_field.elements > 0) _field.elements -= element;
                    _field.offset += TOBITS(element * FIELD_TYPE_BYTES[_field.type]);
                }
            }
        }
        else
        {
            definition_t* subdef = definitions[_field.exttype];
            field_t subfield = getUserField(subdef, subfield_name, _field.flags);
            subfield.offset += _field.offset;
            _field = subfield;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to parse field %s: %s", field_name, e.what());
    }

    /* Return Field */
    _field.flags |= parent_flags;
    return _field;
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
    if(!field_name) return FIELDERR_DEF;

    /* Initialize Parameters */
    recordDefErr_t status = FIELDERR_DEF;
    int end_of_field;
    if(flags & POINTER) end_of_field = offset + FIELD_TYPE_BYTES[INT32];
    else                end_of_field = type == BITFIELD ? TOBYTES(offset + elements) : offset + (elements * FIELD_TYPE_BYTES[type]);
    const int field_offset = type == BITFIELD ? offset : TOBITS(offset);

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
* scanDefinition
*----------------------------------------------------------------------------*/
void RecordObject::scanDefinition (definition_t* def, const char* field_prefix, const char* rec_type)
{
    /* Get Fields in Record */
    const Dictionary<field_t>* fields = getRecordFields(rec_type);
    if(fields == NULL)
    {
        mlog(CRITICAL, "Unable to scan record type: %s\n", rec_type);
        return;
    }

    /* Loop Through Fields in Record */
    Dictionary<field_t>::Iterator field_iter(*fields);
    for(int i = 0; i < field_iter.length; i++)
    {
        const Dictionary<field_t>::kv_t kv = field_iter[i];
        const FString field_name("%s%s%s", field_prefix, strlen(field_prefix) == 0 ? "" : ".", kv.key);
        const field_t& _field = kv.value;

        /* Check for Marked Field */
        if((_field.flags & INDEX)    && (def->meta.index_field == NULL))  def->meta.index_field    = field_name.c_str(true);
        if((_field.flags & TIME)     && (def->meta.time_field == NULL))   def->meta.time_field     = field_name.c_str(true);
        if((_field.flags & X_COORD)  && (def->meta.x_field == NULL))      def->meta.x_field        = field_name.c_str(true);
        if((_field.flags & Y_COORD)  && (def->meta.y_field == NULL))      def->meta.y_field        = field_name.c_str(true);
        if((_field.flags & Z_COORD)  && (def->meta.z_field == NULL))      def->meta.z_field        = field_name.c_str(true);
        if((_field.flags & BATCH)    && (def->meta.batch_field == NULL))  def->meta.batch_field    = field_name.c_str(true);

        /* Recurse for User Fields */
        if(_field.type == USER)
        {
            scanDefinition(def, field_name.c_str(), _field.exttype);
        }
    }
}

/*----------------------------------------------------------------------------
 * getDefinition
 *----------------------------------------------------------------------------*/
RecordObject::definition_t* RecordObject::getDefinition(const char* rec_type)
{
    definition_t* def = NULL;
    try { def = definitions[rec_type]; }
    catch (const RunTimeException& e) { (void)e; }
    return def;
}

/*----------------------------------------------------------------------------
 * getDefinition
 *
 *  Notes:
 *   1. operates on serialized data (not a population string)
 *----------------------------------------------------------------------------*/
RecordObject::definition_t* RecordObject::getDefinition(const unsigned char* buffer, int size)
{
    /* Check Parameters */
    if(buffer == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Null buffer used to retrieve record definition");
    if(size <= (int)sizeof(rec_hdr_t)) throw RunTimeException(CRITICAL, RTE_FAILURE, "Buffer too small to retrieve record definition");

    /* Get Record Definitions */
    const char* rec_type = reinterpret_cast<const char*>(&buffer[sizeof(rec_hdr_t)]);
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
RecordInterface::RecordInterface(const unsigned char* buffer, int size)
{
    recordDefinition = getDefinition(buffer, size);
    if(recordDefinition != NULL)
    {
        if (size >= static_cast<int>(sizeof(rec_hdr_t)))
        {
            const char** recordMemoryCast = const_cast<const char**>(reinterpret_cast<char**>(&recordMemory));
            const unsigned char** recordDataCast = const_cast<const unsigned char**>(&recordData);

            if(parseSerial(buffer, size, recordMemoryCast, recordDataCast) > 0)
            {
                memoryOwner = false;
                memoryAllocated = size;
                memoryUsed = memoryAllocated;
            }
            else throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to differentiate the record type from record data");
        }
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "Buffer passed in not large enough to populate record");
    }
    else throw RunTimeException(CRITICAL, RTE_FAILURE, "Could not find a definition that matches the record buffer");
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RecordInterface::~RecordInterface(void) = default;

// NOLINTEND(misc-no-recursion)
