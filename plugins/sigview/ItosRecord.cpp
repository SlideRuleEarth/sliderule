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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "ItosRecord.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

#include <math.h>
#include <float.h>

using namespace Itos;

/******************************************************************************
 * RECORD CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
Record::Record(bool _is_prototype, const char* _type, const char* _name)
{
    prototype   = _is_prototype;
    type        = StringLib::duplicate(_type);
    name        = StringLib::duplicate(_name);
    comment     = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Record::~Record(void)
{
    if(type) delete [] type;
    if(name) delete [] name;
    if(comment) delete [] comment;
}

/*----------------------------------------------------------------------------
 * addSubRecord  -
 *----------------------------------------------------------------------------*/
void Record::addSubRecord(Record* record)
{
    subrecords.add(record);
}

/*----------------------------------------------------------------------------
 * addValue  -
 *----------------------------------------------------------------------------*/
void Record::addValue(const char* value)
{
    subvalues.add(value);
}

/*----------------------------------------------------------------------------
 * isValue  -
 *----------------------------------------------------------------------------*/
bool Record::isValue(void)
{
    return ((strcmp(type, "#") == 0) || (strcmp(type, "$") == 0));
}

/*----------------------------------------------------------------------------
 * isRedefinition  -
 *----------------------------------------------------------------------------*/
bool Record::isRedefinition(void)
{
    return (strcmp(type, "@") == 0);
}

/*----------------------------------------------------------------------------
 * isType  -
 *----------------------------------------------------------------------------*/
bool Record::isType(const char* typestr)
{
    return (strcmp(type, typestr) == 0);
}

/*----------------------------------------------------------------------------
 * isPrototype  -
 *----------------------------------------------------------------------------*/
bool Record::isPrototype(void)
{
    return prototype;
}

/*----------------------------------------------------------------------------
 * setPrototype  -
 *----------------------------------------------------------------------------*/
void Record::setPrototype(bool _prototype)
{
    prototype = _prototype;
}

/*----------------------------------------------------------------------------
 * setComment  -
 *----------------------------------------------------------------------------*/
void Record::setComment(const char* _comment)
{
    comment = StringLib::duplicate(_comment);
}

/*----------------------------------------------------------------------------
 * getNumSubRecords  -
 *----------------------------------------------------------------------------*/
int Record::getNumSubRecords(void)
{
    return subrecords.length();
}

/*----------------------------------------------------------------------------
 * getNumSubValues  -
 *----------------------------------------------------------------------------*/
int Record::getNumSubValues(void)
{
    return subvalues.length();
}

/*----------------------------------------------------------------------------
 * getSubRecord  -
 *----------------------------------------------------------------------------*/
Record* Record::getSubRecord(int index)
{
    return subrecords[index];
}

/*----------------------------------------------------------------------------
 * getSubValue  -
 *----------------------------------------------------------------------------*/
const char* Record::getSubValue(int index)
{
    return subvalues[index];
}

/*----------------------------------------------------------------------------
 * getName  -
 *----------------------------------------------------------------------------*/
const char* Record::getName(void)
{
    return name;
}

/*----------------------------------------------------------------------------
 * getUnqualifiedName  -
 *----------------------------------------------------------------------------*/
const char* Record::getUnqualifiedName(void)
{
    if(name == NULL) return NULL;
    int len = (int)strlen(name);
    while(len > 0 && name[len-1] != '.') len--;
    return &name[len];
}

/*----------------------------------------------------------------------------
 * getDisplayName  -
 *----------------------------------------------------------------------------*/
const char* Record::getDisplayName(void)
{
    return getUnqualifiedName();
}

/*----------------------------------------------------------------------------
 * getUndottedName  -
 *
 *   Notes: Must free return string
 *----------------------------------------------------------------------------*/
const char* Record::getUndottedName (void)
{
    if(name == NULL) return NULL;
    SafeString s("%s", name);
    s.replace(".", "_");
    s.replace("[", "_");
    s.replace("]", "_");
    return s.getString(true);
}

/*----------------------------------------------------------------------------
 * getNumArrayElements  -
 *----------------------------------------------------------------------------*/
int Record::getNumArrayElements (void)
{
    int num_elements = 1;

    const char* b1 = getUnqualifiedName();
    const char* b2 = NULL;

    while(b1 != NULL)
    {
        b1 = strchr(b1, '[');
        if(b1 != NULL)
        {
            b2 = strchr(b1, ']');
        }

        if(b1 != NULL && b2 != NULL)
        {
            #define MAX_CHARS 256
            char num_elements_str[MAX_CHARS];
            memset(num_elements_str, 0, MAX_CHARS);
            LocalLib::copy(num_elements_str, b1 + 1, b2 - (b1 + 1));
            long num_elements_tmp = 0;
            if(StringLib::str2long(num_elements_str, &num_elements_tmp))
            {
                if(num_elements_tmp != 0)
                {
                    num_elements *= num_elements_tmp;
                }
            }
            b1 = b2;
        }
    }
    return num_elements;
}

/*----------------------------------------------------------------------------
 * getType  -
 *----------------------------------------------------------------------------*/
const char* Record::getType(void)
{
    return type;
}

/*----------------------------------------------------------------------------
 * getComment  -
 *----------------------------------------------------------------------------*/
const char* Record::getComment(void)
{
    return comment;
}

/******************************************************************************
 * COMMAND TYPE CONVERSION CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TypeConversion::TypeConversion(type_conv_t _type, const char* _name)
{
    type = _type;
    name = StringLib::duplicate(_name);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TypeConversion::~TypeConversion(void)
{
    delete [] name;
}

/*----------------------------------------------------------------------------
 * addEnumLookup  -
 *----------------------------------------------------------------------------*/
void TypeConversion::addEnumLookup(const char* enum_name, const char* value)
{
    lookup.add(enum_name, value);
}

/*----------------------------------------------------------------------------
 * getEnumValue  -
 *----------------------------------------------------------------------------*/
const char* TypeConversion::getEnumValue(const char* enum_name)
{
    try
    {
        return lookup.get(enum_name);
    }
    catch(std::out_of_range& e)
    {
        (void)e;
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * getName  -
 *----------------------------------------------------------------------------*/
const char* TypeConversion::getName(void)
{
    return name;
}

/*----------------------------------------------------------------------------
 * getNames  -
 *----------------------------------------------------------------------------*/
int TypeConversion::getNames(char*** names)
{
    return lookup.getKeys(names);
}

/*----------------------------------------------------------------------------
 * getType  -
 *----------------------------------------------------------------------------*/
const char* TypeConversion::getType(void)
{
    switch(type)
    {
        case CMD_ENUM:  return "Enumeration";
        case TLM_CONV:  return "Discrete";
        case EXP_ALGO:  return "Algorithm";
        case EXP_CONV:  return "Conversion";
        case PLY_CONV:  return "Polynomial";
        default:        return "Unknown";
    }
}

/*----------------------------------------------------------------------------
 * isName  -
 *----------------------------------------------------------------------------*/
bool TypeConversion::isName(const char* _name)
{
    if(_name == NULL)   return false;
    else                return (strcmp(_name, name) == 0);
}

/*----------------------------------------------------------------------------
 * getAsHtml  -
 *----------------------------------------------------------------------------*/
const char* TypeConversion::getAsHtml(bool comma_separate)
{
    char* ret_str = NULL;

    if(lookup.length() > 0)
    {
        ret_str = new char[MAX_STR_LEN];
        memset(ret_str, 0, MAX_STR_LEN);
        char tmp[MAX_STR_LEN];
        char** keys;
        lookup.getKeys(&keys);
        for(int e = 0; e < lookup.length(); e++)
        {
            const char* keyptr = keys[e];
            const char* valptr = getEnumValue(keys[e]);
            if(keyptr != NULL && valptr != NULL)
            {
                snprintf(tmp, MAX_STR_LEN, "<br />%s = %s", keyptr, valptr);
                StringLib::concat(ret_str, tmp, MAX_STR_LEN);
                if(e + 1 == lookup.length() || !comma_separate)
                {
                    StringLib::concat(ret_str, "\n", MAX_STR_LEN);
                }
                else
                {
                    StringLib::concat(ret_str, ",\n", MAX_STR_LEN);
                }
            }
            delete [] keyptr;
        }

        delete [] keys;
    }

    return ret_str;
}

/******************************************************************************
 * FIELD CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
Field::Field(field_type_t _field_type, Record* _record, Record* _container, int _container_index, int _num_elements, int _length_in_bits, int _offset_in_bits, int _byte_offset, bool _payload, int _base_size_in_bits, bool _big_endian)
{
    /* Initialize attributes */
    fieldType       = _field_type;
    record          = _record;
    container       = _container;
    containerIndex  = _container_index;
    numElements     = _num_elements;
    lengthInBits    = _length_in_bits;
    offsetInBits    = _offset_in_bits;
    byteOffset      = _byte_offset;
    payload         = _payload;
    baseSizeInBits  = _base_size_in_bits;
    bigEndian       = _big_endian;
    conversion      = NULL;
    rangeChecking   = true;

    /* Calculate Attributes */
    calcAttributes();
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Field::~Field(void)
{
}

/*----------------------------------------------------------------------------
 * setProperty  -
 *
 *   Notes: This is the API to call virtualized _setProperty functions for the various fields
 *----------------------------------------------------------------------------*/
bool Field::setProperty (const char* property, const char* _value, int index)
{
    static bool offsetFrom_beginning = false; // when setting the offset property in ITOS record language, the offset from is a stateful property that applies to the next offset property statement
    bool status = true;

    /* Sanity Check Value */
    if(_value == NULL) return false;

    /* Pre-Parse Temporary Numeric Value */
    long tmpval;
    StringLib::str2long(_value, &tmpval);

    /* Set Property */
    if      (strcmp(property, "bigEndian") == 0)    bigEndian = tmpval == 0 ? false : true;
    else if (strcmp(property, "lengthInBits") == 0) lengthInBits = tmpval;
    else if (strcmp(property, "offsetInBits") == 0) offsetInBits = tmpval;
    else if (strcmp(property, "offsetFrom") == 0)
    {
        if(strcmp("\"startOfPacket\"", _value) == 0)
        {
            offsetFrom_beginning = true;
        }
        else
        {
            offsetFrom_beginning = false;
        }
    }
    else if (strcmp(property, "offset") == 0)
    {
        long offset_val = tmpval;
        if(index == 0)
        {
            if(offsetFrom_beginning)
            {
                offsetInBits = offset_val * 8;
                byteOffset = offset_val;
            }
            else
            {
                offsetInBits += offset_val * 8;
                byteOffset += offset_val;
            }
        }
        else if(index == 1)
        {
            if(offsetFrom_beginning)
            {
                offsetInBits = offset_val;
            }
            else
            {
                offsetInBits += offset_val;
            }
        }

        offsetFrom_beginning = false;
    }
    else if (strcmp(property, "range") == 0)
    {
        char* rngstr = strstr((char*)_value, "..");
        if(rngstr != NULL) // range specified
        {
            char minstr[Record::MAX_VAL_SIZE], maxstr[Record::MAX_VAL_SIZE]; (void)maxstr;
            LocalLib::copy(minstr, _value, rngstr - _value); minstr[rngstr - _value] = 0;
            LocalLib::copy(maxstr, rngstr + 2, strlen(rngstr + 2)); maxstr[strlen(rngstr + 2)] = 0;
            bool stat1 = _setProperty("minRange", minstr, index);
            bool stat2 = _setProperty("maxRange", maxstr, index);
            status = stat1 && stat2;
        }
        else
        {
            bool stat1 = _setProperty("minRange", _value, index);
            bool stat2 = _setProperty("maxRange", _value, index);
            bool stat3 = _setProperty("defaultValue", _value, index);
            status = stat1 && stat2 && stat3;
        }
    }
    else if (strcmp(property, "rangeOn") == 0)
    {
        if (strcmp(_value, "\"unconverted\"") == 0)
        {
            rangeChecking = false;
        }
    }
    else if (strcmp(property, "enumeration") == 0)
    {
        conversion = _value;
    }
    else if (strcmp(property, "conversion") == 0)
    {
        conversion = _value;
    }
    else
    {
        status = _setProperty(property, _value, index);
    }

    /* Update Internal Attributes */
    if(status == true)
    {
        calcAttributes();
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * getProperty  -
 *
 *   Notes: this is the API to call virtualized _getProperty functions of the various fields
 *----------------------------------------------------------------------------*/
const char* Field::getProperty (const char* property, int index)
{
    char* valstr = new char[Record::MAX_VAL_SIZE];

    if      (strcmp(property, "lengthInBits") == 0) snprintf(valstr, Record::MAX_VAL_SIZE, "%d", lengthInBits);
    else if (strcmp(property, "offsetInBits") == 0) snprintf(valstr, Record::MAX_VAL_SIZE, "%d", offsetInBits);
    else if (strcmp(property, "enumeration") == 0)  snprintf(valstr, Record::MAX_VAL_SIZE, "%s", conversion);
    else if (strcmp(property, "conversion") == 0)   snprintf(valstr, Record::MAX_VAL_SIZE, "%s", conversion);
    else
    {
        delete [] valstr;
        return _getProperty(property, index);
    }

    return valstr;
}

/*----------------------------------------------------------------------------
 * getDisplayName  -
 *----------------------------------------------------------------------------*/
const char* Field::getDisplayName (char* buf)
{
    char* namestr = buf;

    if(namestr == NULL)
    {
        namestr = new char[Record::MAX_TOKEN_SIZE];
    }

    memset(namestr, 0, Record::MAX_TOKEN_SIZE);

    if(record->getDisplayName() != NULL)
    {
        if(container == NULL)
        {
            snprintf(namestr, Record::MAX_TOKEN_SIZE, "%s", record->getDisplayName());
        }
        else if(container->getDisplayName() != NULL)
        {
            if(strchr(container->getDisplayName(), '[') != NULL)
            {
                char tmp[Record::MAX_TOKEN_SIZE];
                snprintf(tmp, Record::MAX_TOKEN_SIZE, "%s", container->getDisplayName());
                char* b = strchr(tmp, '[');
                *b = '\0'; // terminate name string at first bracket
                snprintf(namestr, Record::MAX_TOKEN_SIZE, "%s[%d].%s", tmp, containerIndex, record->getDisplayName());
            }
            else
            {
                snprintf(namestr, Record::MAX_TOKEN_SIZE, "%s.%s", container->getDisplayName(), record->getDisplayName());
            }
        }
    }

    return namestr;
}

/*----------------------------------------------------------------------------
 * getUnqualifiedName  -
 *----------------------------------------------------------------------------*/
const char* Field::getUnqualifiedName (void)
{
    if(record != NULL)
    {
        return record->getUnqualifiedName();
    }
    else
    {
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * getUndottedName  -
 *----------------------------------------------------------------------------*/
const char* Field::getUndottedName (void)
{
    if(record != NULL)
    {
        return record->getUndottedName();
    }
    else
    {
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * getName  -
 *----------------------------------------------------------------------------*/
const char* Field::getName (void)
{
    if(record != NULL)
    {
        return record->getName();
    }
    else
    {
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * getType  -
 *----------------------------------------------------------------------------*/
const char* Field::getType (void)
{
    if(record != NULL)
    {
        return record->getType();
    }
    else
    {
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * getOffsetInBits  -
 *----------------------------------------------------------------------------*/
int Field::getOffsetInBits (void)
{
    return offsetInBits;
}

/*----------------------------------------------------------------------------
 * getByteOffset  -
 *----------------------------------------------------------------------------*/
int Field::getByteOffset (void)
{
    return byteOffset;
}

/*----------------------------------------------------------------------------
 * getLengthInBits  -
 *----------------------------------------------------------------------------*/
int Field::getLengthInBits (void)
{
    return lengthInBits;
}

/*----------------------------------------------------------------------------
 * getNumElements  -
 *----------------------------------------------------------------------------*/
int Field::getNumElements (void)
{
    return numElements;
}

/*----------------------------------------------------------------------------
 * getByteSize  -
 *----------------------------------------------------------------------------*/
int Field::getByteSize (void)
{
    return byteSize;
}

/*----------------------------------------------------------------------------
 * getBaseSizeInBits  -
 *----------------------------------------------------------------------------*/
int Field::getBaseSizeInBits (void)
{
    return baseSizeInBits;
}

/*----------------------------------------------------------------------------
 * getBitMask  -
 *----------------------------------------------------------------------------*/
unsigned long Field::getBitMask (void)
{
    return bitMask;
}

/*----------------------------------------------------------------------------
 * getConversion  -
 *----------------------------------------------------------------------------*/
const char* Field::getConversion (void)
{
    return conversion;
}

/*----------------------------------------------------------------------------
 * getBigEndian  -
 *----------------------------------------------------------------------------*/
bool Field::getBigEndian (void)
{
    return bigEndian;
}

/*----------------------------------------------------------------------------
 * getComment  -
 *----------------------------------------------------------------------------*/
const char* Field::getComment (void)
{
    return record->getComment();
}

/*----------------------------------------------------------------------------
 * isName  -
 *----------------------------------------------------------------------------*/
bool Field::isName (const char* namestr)
{
    if(record != NULL)
    {
        const char* field_name = record->getName();
        if(field_name != NULL)
        {
            return (strcmp(field_name, namestr) == 0);
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * isPayload  -
 *----------------------------------------------------------------------------*/
bool Field::isPayload (void)
{
    return payload;
}

/*----------------------------------------------------------------------------
 * isType  -
 *----------------------------------------------------------------------------*/
bool Field::isType (field_type_t type)
{
    return (type == fieldType);
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
Field* Field::duplicate (void)
{
    return NULL;
}

/*----------------------------------------------------------------------------
 * _setProperty  -
 *
 *   Notes: virtual, called through non-virtual setProperty function
 *----------------------------------------------------------------------------*/
bool Field::_setProperty (const char* property, const char* _value, int index)
{
    (void)property;
    (void)_value;
    (void)index;

    return false;
}

/*----------------------------------------------------------------------------
 * _getProperty  -
 *
 *   Notes: virtual, called through non-virtual getProperty function
 *----------------------------------------------------------------------------*/
const char* Field::_getProperty (const char* property, int index)
{
    (void)property;
    (void)index;
    return NULL;
}

/*----------------------------------------------------------------------------
 * getRawValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
unsigned long Field::getRawValue (int element)
{
    (void)element;
    return 0;
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* Field::getStrValue (int element)
{
    (void)element;
    return NULL;
}

/*----------------------------------------------------------------------------
 * populate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
bool Field::populate (unsigned char* pkt)
{
    (void)pkt;
    return false;
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *
 *   Notes: PRIVATE METHOD
 *----------------------------------------------------------------------------*/
void Field::calcAttributes (void)
{
    /* Calculate Bit Mask */
    unsigned long mask = 0x0;
    if(baseSizeInBits > 0)
    {
        unsigned long upper_bit = 1L << (baseSizeInBits - 1);
        for(int b = 0; b < lengthInBits; b++)
        {
            mask >>= 1;
            mask |= upper_bit;
        }
        mask >>= (offsetInBits - (byteOffset * 8));
    }
    bitMask = mask;

    /* Calculate Byte Size */
    byteSize = ((lengthInBits * numElements) + (offsetInBits % 8) + 7) / 8;
}

/******************************************************************************
 * INTEGER FIELD SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
IntegerField::IntegerField( Record* _record,
                            Record* _container,
                            int     _container_index,
                            int     _num_elements,
                            int     _length_in_bits,
                            int     _offset_in_bits,
                            int     _byte_offset,
                            long    _default_value,
                            long    _min_range,
                            long    _max_range,
                            bool    _payload,
                            int     _base_size_in_bits,
                            bool    _big_endian):

                            Field(  INTEGER,
                                    _record,
                                    _container,
                                    _container_index,
                                    _num_elements,
                                    _length_in_bits,
                                    _offset_in_bits,
                                    _byte_offset,
                                    _payload,
                                    _base_size_in_bits,
                                    _big_endian)
{
    minRange    = _min_range;
    maxRange    = _max_range;

    assert(_num_elements > 0);
    value = new long[_num_elements];
    for(int i = 0; i < _num_elements; i++)
    {
        value[i] = _default_value;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
IntegerField::~IntegerField(void)
{
    delete [] value;
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
Field* IntegerField::duplicate(void)
{
    Field* field = new IntegerField(record, container, containerIndex, numElements, lengthInBits, offsetInBits, byteOffset, value[0], minRange, maxRange, payload, baseSizeInBits, bigEndian);
    field->setProperty("conversion", conversion, 0);
    return field;
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
unsigned long IntegerField::getRawValue (int element)
{
    assert(element < numElements);
    return (unsigned long)value[element];
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* IntegerField::getStrValue (int element)
{
    assert(element < numElements);

    char* valstr = new char [Record::MAX_VAL_SIZE];
    snprintf(valstr, Record::MAX_VAL_SIZE, "%ld", value[element]);

    return valstr;
}

/*----------------------------------------------------------------------------
 * _setProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
bool IntegerField::_setProperty(const char* property, const char* _value, int index)
{
    assert(index < numElements);

    long tmpval = 0;
    if(!StringLib::str2long(_value, &tmpval))
    {
        return false;
    }

         if (strcmp(property, "defaultValue") == 0) value[index] = tmpval;
    else if (strcmp(property, "value") == 0)        value[index] = tmpval;
    else if (strcmp(property, "minRange") == 0)     minRange     = tmpval;
    else if (strcmp(property, "maxRange") == 0)     maxRange     = tmpval;
    else    return false;
    return  true;
}

/*----------------------------------------------------------------------------
 * _getProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
const char* IntegerField::_getProperty(const char* property, int index)
{
    assert(index < numElements);

    char* valstr = new char[Record::MAX_VAL_SIZE];

         if (strcmp(property, "defaultValue") == 0) snprintf(valstr, Record::MAX_VAL_SIZE, "%ld", value[index]);
    else if (strcmp(property, "value") == 0)        snprintf(valstr, Record::MAX_VAL_SIZE, "%ld", value[index]);
    else if (strcmp(property, "minRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%ld", minRange);
    else if (strcmp(property, "maxRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%ld", maxRange);
    else
    {
        delete [] valstr;
        return NULL;
    }

    return valstr;
}

/*----------------------------------------------------------------------------
 * populate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
bool IntegerField::populate (unsigned char* pkt)
{
    bool status = true;

    for(int n = 0; n < numElements; n++)
    {
        /* Setup Parameters */
        int bits_left = lengthInBits;
        int bits_to_lsb = (lengthInBits * (n + 1)) + offsetInBits;
        int initial_shift = (sizeof(long) - bits_to_lsb) % 8;
        int byte_index = (bits_to_lsb - 1) / 8;
        if(byte_index >= CCSDS_GET_LEN(pkt) || byte_index < 0)
        {
            mlog(ERROR, "Failed to populate field %s from packet %04X due to size mismatch (%d, %d)\n", record->getName(), CCSDS_GET_SID(pkt), byte_index, CCSDS_GET_LEN(pkt));
            status = false;
        }
        else
        {
            /* Decompose Packet */
            int i = 0;
            unsigned long _value = 0;
            while(bits_left > 0)
            {
                unsigned int contribution = pkt[byte_index--] >> initial_shift;
                unsigned int byte_mask = (1 << MIN(8, bits_left)) - 1;
                _value += (contribution & byte_mask) * (1L << (i++ * 8));
                bits_left -= 8;
                initial_shift = 0;
            }

            /* Set Value */
            long candidate = (long)_value;
            if(!rangeChecking || (candidate >= minRange && candidate <= maxRange))
            {
                value[n] = candidate;
            }
            else
            {
                mlog(ERROR, "Failed to populate field %s from packet %04X due to out of bounds input %ld [%ld, %ld]\n", record->getName(), CCSDS_GET_SID(pkt), candidate, minRange, maxRange);
                status = false;
            }
        }
    }

    return status;
}

/******************************************************************************
 * UNSIGNED INTEGER FIELD SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UnsignedField::UnsignedField(   Record*         _record,
                                Record*         _container,
                                int             _container_index,
                                int             _num_elements,
                                int             _length_in_bits,
                                int             _offset_in_bits,
                                int             _byte_offset,
                                unsigned long   _default_value,
                                unsigned long   _min_range,
                                unsigned long   _max_range,
                                bool            _payload,
                                int             _base_size_in_bits,
                                bool            _big_endian):

                                Field(  UNSIGNED,
                                        _record,
                                        _container,
                                        _container_index,
                                        _num_elements,
                                        _length_in_bits,
                                        _offset_in_bits,
                                        _byte_offset,
                                        _payload,
                                        _base_size_in_bits,
                                        _big_endian)
{
    minRange    = _min_range;
    maxRange    = _max_range;

    assert(_num_elements > 0);
    value = new unsigned long[_num_elements];
    for(int i = 0; i < _num_elements; i++)
    {
        value[i] = _default_value;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UnsignedField::~UnsignedField(void)
{
    delete [] value;
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
Field* UnsignedField::duplicate(void)
{
    Field* field = new UnsignedField(record, container, containerIndex, numElements, lengthInBits, offsetInBits, byteOffset, value[0], minRange, maxRange, payload, baseSizeInBits, bigEndian);
    field->setProperty("conversion", conversion, 0);
    return field;
}

/*----------------------------------------------------------------------------
 * getRawValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
unsigned long UnsignedField::getRawValue (int element)
{
    assert(element < numElements);
    return value[element];
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* UnsignedField::getStrValue (int element)
{
    assert(element < numElements);

    char* valstr = new char [Record::MAX_VAL_SIZE];
    snprintf(valstr, Record::MAX_VAL_SIZE, "%lu", value[element]);

    return valstr;
}

/*----------------------------------------------------------------------------
 * _setProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
bool UnsignedField::_setProperty(const char* property, const char* _value, int index)
{
    assert(index < numElements);

    unsigned long tmpval = 0;
    if(!StringLib::str2ulong(_value, &tmpval))
    {
        return false;
    }

         if (strcmp(property, "defaultValue") == 0) value[index]    = tmpval;
    else if (strcmp(property, "value") == 0)        value[index]    = tmpval;
    else if (strcmp(property, "minRange") == 0)     minRange        = tmpval;
    else if (strcmp(property, "maxRange") == 0)     maxRange        = tmpval;
    else    return false;
    return  true;
}

/*----------------------------------------------------------------------------
 * _getProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
const char* UnsignedField::_getProperty(const char* property, int index)
{
    assert(index < numElements);

    char* valstr = new char[Record::MAX_VAL_SIZE];

         if (strcmp(property, "defaultValue") == 0) snprintf(valstr, Record::MAX_VAL_SIZE, "%lu", value[index]);
    else if (strcmp(property, "value") == 0)        snprintf(valstr, Record::MAX_VAL_SIZE, "%lu", value[index]);
    else if (strcmp(property, "minRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%lu", minRange);
    else if (strcmp(property, "maxRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%lu", maxRange);
    else
    {
        delete [] valstr;
        return NULL;
    }

    return valstr;
}

/*----------------------------------------------------------------------------
 * populate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
bool UnsignedField::populate (unsigned char* pkt)
{
    bool status = true;

    for(int n = 0; n < numElements; n++)
    {
        /* Setup Parameters */
        int bits_left = lengthInBits;
        int bits_to_lsb = (lengthInBits * (n + 1)) + offsetInBits;
        int bits_to_shift = bits_to_lsb % 8;
        int byte_index = (bits_to_lsb - 1) / 8;
        if(byte_index >= CCSDS_GET_LEN(pkt) || byte_index < 0)
        {
            mlog(ERROR, "Failed to populate field %s from packet %04X due to size mismatch (%d, %d)\n", record->getName(), CCSDS_GET_SID(pkt), byte_index, CCSDS_GET_LEN(pkt));
            status = false;
        }
        else
        {
            /* Decompose Packet */
            int i = 0;
            unsigned long _value = 0;
            while(bits_left > 0)
            {
                int byte_mask = (1 << MIN(8, bits_left)) - 1;
                _value += (pkt[byte_index--] & byte_mask) * (unsigned long)pow(2, bits_to_shift + (i++ * 8));
                bits_left -= 8;
            }

            /* Set Value */
            if(!rangeChecking || (_value >= minRange && _value <= maxRange))
            {
                value[n] = _value;
            }
            else
            {
                mlog(ERROR, "Failed to populate field %s from packet %04X due to out of bounds input %lu [%lu, %lu]\n", record->getName(), CCSDS_GET_SID(pkt), _value, minRange, maxRange);
                status = false;
            }
        }
    }

    return status;
}

/******************************************************************************
 *FLOAT FIELD SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
FloatField::FloatField( Record* _record,
                        Record* _container,
                        int     _container_index,
                        int     _num_elements,
                        int     _length_in_bits,
                        int     _offset_in_bits,
                        int     _byte_offset,
                        double  _default_value,
                        double  _min_range,
                        double  _max_range,
                        bool    _payload,
                        int     _base_size_in_bits,
                        bool    _big_endian):

                        Field(  FLOAT,
                                _record,
                                _container,
                                _container_index,
                                _num_elements,
                                _length_in_bits,
                                _offset_in_bits,
                                _byte_offset,
                                _payload,
                                _base_size_in_bits,
                                _big_endian)
{
    minRange    = _min_range;
    maxRange    = _max_range;

    assert(_num_elements > 0);
    value = new double[_num_elements];
    for(int i = 0; i < _num_elements; i++)
    {
        value[i] = _default_value;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
FloatField::~FloatField(void)
{
    delete [] value;
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
Field* FloatField::duplicate(void)
{
    Field* field = new FloatField(record, container, containerIndex, numElements, lengthInBits, offsetInBits, byteOffset, value[0], minRange, maxRange, payload, baseSizeInBits, bigEndian);
    field->setProperty("conversion", conversion, 0);
    return field;
}

/*----------------------------------------------------------------------------
 * getRawValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
unsigned long FloatField::getRawValue (int element)
{
    assert(element < numElements);
    double val = value[element];
    unsigned long* raw_ptr = (unsigned long*)&val;
    return *raw_ptr;
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* FloatField::getStrValue (int element)
{
    assert(element < numElements);

    char* valstr = new char [Record::MAX_VAL_SIZE];
    snprintf(valstr, Record::MAX_VAL_SIZE, "%lf", value[element]);

    return valstr;
}

/*----------------------------------------------------------------------------
 * _setProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
bool FloatField::_setProperty(const char* property, const char* _value, int index)
{
    assert(index < numElements);

    double tmpval = 0;
    if(!StringLib::str2double(_value, &tmpval))
    {
        return false;
    }

         if (strcmp(property, "defaultValue") == 0) value[index]    = tmpval;
    else if (strcmp(property, "value") == 0)        value[index]    = tmpval;
    else if (strcmp(property, "minRange") == 0)     minRange        = tmpval;
    else if (strcmp(property, "maxRange") == 0)     maxRange        = tmpval;
    else    return false;
    return  true;
}

/*----------------------------------------------------------------------------
 * _getProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
const char* FloatField::_getProperty(const char* property, int index)
{
    assert(index < numElements);

    char* valstr = new char[Record::MAX_VAL_SIZE];

         if (strcmp(property, "defaultValue") == 0) snprintf(valstr, Record::MAX_VAL_SIZE, "%.6lf", value[index]);
    else if (strcmp(property, "value") == 0)        snprintf(valstr, Record::MAX_VAL_SIZE, "%.6lf", value[index]);
    else if (strcmp(property, "minRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%.6lf", minRange);
    else if (strcmp(property, "maxRange") == 0)     snprintf(valstr, Record::MAX_VAL_SIZE, "%.6lf", maxRange);
    else
    {
        delete [] valstr;
        return NULL;
    }

    return valstr;
}

/*----------------------------------------------------------------------------
 * populate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
bool FloatField::populate (unsigned char* pkt)
{
    bool status = true;

    for(int n = 0; n < numElements; n++)
    {
        /* Setup Parameters */
        int bits_left = lengthInBits;
        int bits_to_lsb = (lengthInBits * (n + 1)) + offsetInBits;
        int bits_to_shift = bits_to_lsb % 8;
        int byte_index = (bits_to_lsb - 1) / 8;
        if(byte_index >= CCSDS_GET_LEN(pkt) || byte_index < 0)
        {
            mlog(ERROR, "Failed to populate field %s from packet %04X due to size mismatch (%d, %d)\n", record->getName(), CCSDS_GET_SID(pkt), byte_index, CCSDS_GET_LEN(pkt));
            status = false;
        }
        else
        {
            /* Decompose Packet */
            int i = 0;
            unsigned long _value = 0;
            while(bits_left > 0)
            {
                int byte_mask = (1 << MIN(8, bits_left)) - 1;
                _value += (pkt[byte_index--] & byte_mask) * (unsigned long)pow(2, bits_to_shift + (i++ * 8));
                bits_left -= 8;
            }

            /* Set Value */
            double* candidate = (double*)&_value;
            if(!rangeChecking || (*candidate >= minRange && *candidate <= maxRange))
            {
                value[n] = *candidate;
            }
            else
            {
                mlog(ERROR, "Failed to populate field %s from packet %04X due to out of bounds input %lf [%lf, %lf]\n", record->getName(), CCSDS_GET_SID(pkt), *candidate, minRange, maxRange);
                status = false;
            }
        }
    }

    return status;
}

/******************************************************************************
 * STRING FIELD SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
StringField::StringField(   Record*     _record,
                            Record*     _container,
                            int         _container_index,
                            int         _num_elements,
                            int         _length_in_bits,
                            int         _offset_in_bits,
                            int         _byte_offset,
                            const char* _default_value,
                            bool        _payload,
                            int         _base_size_in_bits,
                            bool        _big_endian):

                            Field(  STRING,
                                    _record,
                                    _container,
                                    _container_index,
                                    _num_elements,
                                    _length_in_bits,
                                    _offset_in_bits,
                                    _byte_offset,
                                    _payload,
                                    _base_size_in_bits,
                                    _big_endian)
{
    StringLib::copy(value, _default_value, Record::MAX_TOKEN_SIZE);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
StringField::~StringField(void)
{
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
Field* StringField::duplicate(void)
{
    Field* field = new StringField(record, container, containerIndex, numElements, lengthInBits, offsetInBits, byteOffset, value, payload, baseSizeInBits, bigEndian);
    field->setProperty("conversion", conversion, 0);
    return field;
}

/*----------------------------------------------------------------------------
 * getRawValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
unsigned long StringField::getRawValue (int element)
{
    if(element < numElements)
    {
        return (unsigned long)value[element];
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * getStrValue  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* StringField::getStrValue (int element)
{
    (void)element;

    char* valstr = new char [Record::MAX_TOKEN_SIZE];
    snprintf(valstr, Record::MAX_TOKEN_SIZE, "%s", value);
    valstr[Record::MAX_TOKEN_SIZE - 1] = '\0';

    return valstr;
}

/*----------------------------------------------------------------------------
 * _setProperty  -
 *----------------------------------------------------------------------------*/
bool StringField::_setProperty(const char* property, const char* _value, int index)
{
    (void) index;

    if      (strcmp(property, "defaultValue") == 0)         StringLib::copy(value, _value, Record::MAX_TOKEN_SIZE);
    else if (strcmp(property, "value") == 0)                StringLib::copy(value, _value, Record::MAX_TOKEN_SIZE);
    else if (strcmp(property, "lengthInCharacters") == 0)
    {
        long tmpval = 0;
        if(!StringLib::str2long(_value, &tmpval))    return false;
        else                                        numElements = tmpval;
    }
    else    return false;
    return  true;
}

/*----------------------------------------------------------------------------
 * _getProperty  -
 *
 *   Notes: virtual, called from API setProperty function
 *----------------------------------------------------------------------------*/
const char* StringField::_getProperty(const char* property, int index)
{
    (void) index;

    char* valstr = new char[Record::MAX_TOKEN_SIZE];

    if      (strcmp(property, "defaultValue") == 0) snprintf(valstr, Record::MAX_TOKEN_SIZE, "%s", value);
    else if (strcmp(property, "value") == 0)        snprintf(valstr, Record::MAX_TOKEN_SIZE, "%s", value);
    else
    {
        delete [] valstr;
        return NULL;
    }

    return valstr;
}

/*----------------------------------------------------------------------------
 * populate  -
 *
 *   Notes: virtual, only supports strings on even byte boundaries
 *----------------------------------------------------------------------------*/
bool StringField::populate (unsigned char* pkt)
{
    int pktlen = CCSDS_GET_LEN(pkt);
    if(byteOffset + numElements > pktlen)
    {
        mlog(ERROR, "Failed to populate field %s from packet %04X due to size mismatch (%d > %d)\n", record->getName(), CCSDS_GET_SID(pkt), byteOffset + numElements, pktlen);
        return false;
    }
    else if(byteOffset + numElements > Record::MAX_TOKEN_SIZE)
    {
        mlog(ERROR, "Failed to populate field %s from packet %04X due to size exceeding maximum allowed (%d > %d)\n", record->getName(), CCSDS_GET_SID(pkt), byteOffset + numElements, Record::MAX_TOKEN_SIZE);
        return false;
    }

    for(int i = 0; i < numElements; i++)
    {
        value[i] = (char)pkt[i + byteOffset];
    }

    return true;
}

/******************************************************************************
 * FILTER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
Filter::Filter (    int          _q,
                    int          _spw,
                    const char*  _fsw_define,
                    int          _sid,
                    double       _rate,
                    const char*  _type,
                    const char*  _sender,
                    const char*  _task,
                    const char** _sources)
{
    q       = _q;
    spw     = _spw;
    sid     = _sid;
    rate    = _rate;
    snprintf(fsw_define,    Filter::MAX_STR_LEN, "%s", _fsw_define);
    snprintf(type,          Filter::MAX_STR_LEN, "%s", _type);
    snprintf(sender,        Filter::MAX_STR_LEN, "%s", _sender);
    snprintf(task,          Filter::MAX_STR_LEN, "%s", _task);

    if(_sources != NULL)
    {
        int s = 0;
        while(_sources[s] != NULL)
        {
            SafeString ss("%s", _sources[s]);
            source.add(ss);
            s++;
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Filter::~Filter (void)
{
}

/*----------------------------------------------------------------------------
 * getProperty  -
 *----------------------------------------------------------------------------*/
const char* Filter::getProperty (const char* name)
{
    char* str = new char[MAX_STR_LEN];
    memset(str, 0, MAX_STR_LEN);

    if      (strcmp(name, "q") == 0)            snprintf(str, MAX_STR_LEN, "%d", q);
    else if (strcmp(name, "spw") == 0)          snprintf(str, MAX_STR_LEN, "%d", spw);
    else if (strcmp(name, "fsw_define") == 0)   snprintf(str, MAX_STR_LEN, "%s", fsw_define);
    else if (strcmp(name, "sid") == 0)          snprintf(str, MAX_STR_LEN, "%04X", sid);
    else if (strcmp(name, "rate") == 0)
    {
        if(rate == 0.0) snprintf(str, MAX_STR_LEN, "by cmd");
        else            snprintf(str, MAX_STR_LEN, "%.2lf", rate);
    }
    else if (strcmp(name, "type") == 0)         snprintf(str, MAX_STR_LEN, "%s", type);
    else if (strcmp(name, "sender") == 0)       snprintf(str, MAX_STR_LEN, "%s", sender);
    else if (strcmp(name, "task") == 0)         snprintf(str, MAX_STR_LEN, "%s", task);
    else if (strcmp(name, "source") == 0)
    {
        for(int s = 0; s < source.length(); s++)
        {
           StringLib::concat(str, source[s].getString(), MAX_STR_LEN);
           StringLib::concat(str, " ", MAX_STR_LEN);
        }
    }
    else if (strcmp(name, "rtrate") == 0)
    {
        if(rate == 0.0 || q == 0)   snprintf(str, MAX_STR_LEN, "by cmd");
        else                        snprintf(str, MAX_STR_LEN, "%.2lf", rate / q);
    }
    else
    {
        snprintf(str, MAX_STR_LEN, "---");
    }

    return (const char*)str;
}

/*----------------------------------------------------------------------------
 * onApid  -
 *----------------------------------------------------------------------------*/
bool Filter::onApid (int apid)
{
    return (apid == (sid & 0x7FF));
}


/******************************************************************************
 * PACKET CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * STATIC DATA
 *----------------------------------------------------------------------------*/

const char  Packet::parmSymByte[]   = {'G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
const char  Packet::parmSymBit[]    = {'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
Packet::Packet(packet_type_t _packet_type, bool _populate, const char* _apid_designation)
{
    packetType      = _packet_type;
    declaration     = NULL;
    numBytes        = 6;
    name            = NULL;
    currBitOffset   = 0;
    currByteOffset  = 0;

    packetApidDesignation = StringLib::duplicate(_apid_designation);

    if(_populate)
    {
        Record* ccsdsVersion        = new Record(false, "U12", "ccsdsVersion");
        Record* ccsdsCmdTlm         = new Record(false, "U12", "packetType");
        Record* secondaryHeader     = new Record(false, "U12", "secondaryHeader");
        Record* applicationId       = new Record(false, "U12", packetApidDesignation);
        Record* segmentationFlags   = new Record(false, "U12", "segmentationFlags");
        Record* sequenceCount       = new Record(false, "U12", "sequenceCount");
        Record* length              = new Record(false, "U12", "length");

        Field* ccsdsVersionFld      = new UnsignedField(ccsdsVersion,       NULL, 0, 1,    3,    0,   0,  0, 0, 7,      false, 16, true);
        Field* ccsdsCmdTlmFld       = new UnsignedField(ccsdsCmdTlm,        NULL, 0, 1,    1,    3,   0,  0, 0, 1,      false, 16, true);
        Field* secondaryHeaderFld   = new UnsignedField(secondaryHeader,    NULL, 0, 1,    1,    4,   0,  1, 0, 1,      false, 16, true);    // all atlas packets have secondary headers
        Field* applicationIdFld     = new UnsignedField(applicationId,      NULL, 0, 1,   11,    5,   0,  0, 0, 0x7FF,  false, 16, true);
        Field* segmentationFlagsFld = new UnsignedField(segmentationFlags,  NULL, 0, 1,    2,   16,   2,  3, 0, 3,      false, 16, true);    // by default packets are not segmented
        Field* sequenceCountFld     = new UnsignedField(sequenceCount,      NULL, 0, 1,   14,   18,   2,  0, 0, 0x3FFF, false, 16, true);
        Field* lengthFld            = new UnsignedField(length,             NULL, 0, 1,   16,   32,   4,  0, 0, 0xFFFF, false, 16, true);

        fields.add(ccsdsVersionFld);
        fields.add(ccsdsCmdTlmFld);
        fields.add(secondaryHeaderFld);
        fields.add(applicationIdFld);
        fields.add(segmentationFlagsFld);
        fields.add(sequenceCountFld);
        fields.add(lengthFld);

        orphanRecs.add(ccsdsVersion);
        orphanRecs.add(ccsdsCmdTlm);
        orphanRecs.add(secondaryHeader);
        orphanRecs.add(applicationId);
        orphanRecs.add(segmentationFlags);
        orphanRecs.add(sequenceCount);
        orphanRecs.add(length);

        currBitOffset   = 48;
        currByteOffset  = 6;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Packet::~Packet(void)
{
    for(int i = 0; i < orphanRecs.length(); i++) orphanFree(orphanRecs[i]);
    if(name) delete [] name;
    if(packetApidDesignation) delete [] packetApidDesignation;
}

/*----------------------------------------------------------------------------
 * addField  -
 *
 *   Notes: TODO: this function needs to migrate into RecordParser due to record language specific type specification
 *----------------------------------------------------------------------------*/
void Packet::addField ( Record*             record,
                        Record*             container,
                        int                 container_index,
                        Field::field_type_t type,
                        int                 size_in_bits,
                        bool                big_endian)
{
    assert(record != NULL);

    /* Check for Array */
    int num_elements = record->getNumArrayElements();

    /* Create Field */
    Field* field = NULL;
    if      (type == Field::INTEGER)    field = new IntegerField    (record, container, container_index, num_elements, size_in_bits,  currBitOffset, currByteOffset, 0,   INT_MIN,  INT_MAX,  true, size_in_bits, big_endian);
    else if (type == Field::UNSIGNED)   field = new UnsignedField   (record, container, container_index, num_elements, size_in_bits,  currBitOffset, currByteOffset, 0,   0,        UINT_MAX, true, size_in_bits, big_endian);
    else if (type == Field::FLOAT)      field = new FloatField      (record, container, container_index, num_elements, size_in_bits,  currBitOffset, currByteOffset, 0.0, -DBL_MAX, DBL_MAX,  true, size_in_bits, big_endian);
    else if (type == Field::STRING)     field = new StringField     (record, container, container_index, num_elements, size_in_bits,  currBitOffset, currByteOffset, "",                      true, size_in_bits, big_endian);
    else
    {
        mlog(ERROR, "Unsupported type <%s> in record <%s>\n", record->getType(), record->getName());
        return;
    }

    /* set all the properties */
    for(int j = 0; j < record->getNumSubRecords(); j++)
    {
        Record* proprec = record->getSubRecord(j);

        /* Check that record is marked as a value */
        if(proprec->isValue() == false)
        {
            mlog(WARNING, "Ignored property <%s> of record <%s>\n", proprec->getName(), record->getName());
            continue;
        }

        /* Load Properties */
        const char* property = proprec->getUnqualifiedName();
        for(int k = 0; k < proprec->getNumSubValues(); k++)
        {
            const char* val = (const char*)proprec->getSubValue(k);

            /* Check value is populated in the record */
            if(val == NULL)
            {
                mlog(CRITICAL, "Unable to parse redefinition of record <%s> for value <%s>\n", record->getName(), proprec->getName());
                continue;
            }

            /* Set Property */
            if(field->setProperty(property, val, k) == true)
            {
                mlog(DEBUG, "Setting record <%s> property <%s> to value <%s>\n", record->getName(), property, val);
            }
            else
            {
                mlog(WARNING, "Was not able set property <%s> in record <%s>\n", property, record->getName());
            }
        }
    }

    /* Append the field to the packet */
    fields.add(field);
    currBitOffset = field->getOffsetInBits();
    currByteOffset = field->getByteOffset();

    /* Calculate next field position */
    currBitOffset += (field->getLengthInBits() * field->getNumElements());
    if(currBitOffset >= ((currByteOffset * 8) + field->getBaseSizeInBits()))
    {
        currByteOffset = currBitOffset / 8;
    }

    /* Calculate Packet Length */
    int potential_length = (currBitOffset + 7) / 8;
    if(potential_length > numBytes)
    {
        numBytes = potential_length;
    }

    /* Update Attributes */
    calcAttributes();
}

/*----------------------------------------------------------------------------
 * serialize  -
 *----------------------------------------------------------------------------*/
char* Packet::serialize (serialization_format_t fmt, int max_str_len)
{
    char* serial = NULL;

    /* Process Various Formats */
    if(fmt == BINARY_FMT || fmt == RAW_STOL_CMD_FMT)
    {
        /* Build Binary Packet from Fields */
        /* -- need to de-allocate below when finished with it */
        unsigned char* pkt_bytes = new unsigned char[numBytes];
        memset(pkt_bytes, 0, numBytes);
        for(int p = 0; p < fields.length(); p++)
        {
            Field* field = fields[p];
            for(int e = 0; e < field->getNumElements(); e++)
            {
                int bits_remaining = field->getLengthInBits();
                int bytes_remaining = field->getByteSize();
                while(bits_remaining > 0)
                {
                    int out_byte = (field->getOffsetInBits() + (e * field->getBaseSizeInBits()) + bits_remaining) / 8;
                    int bits_in_out_byte = (field->getOffsetInBits() + (e * field->getBaseSizeInBits()) + bits_remaining) % 8;

                    if(bits_in_out_byte == 0)
                    {
                        bits_in_out_byte = 8;
                        out_byte--;
                    }

                    pkt_bytes[out_byte] |= ((field->getRawValue(e) >> ((field->getByteSize() - bytes_remaining) * 8)) << (8 - bits_in_out_byte)) & 0xFF;;

                    bits_remaining -= bits_in_out_byte;
                    bytes_remaining -= 1;
                }
            }
        }

        /* Select Format */
        if(fmt == BINARY_FMT)
        {
            serial = (char*)pkt_bytes;
        }
        else if(fmt == RAW_STOL_CMD_FMT)
        {
            serial = new char[max_str_len];

            /* Build Packet String with Symbols */
            /* Special Case: CCSDS Length Field is (-7) */
            unsigned int length = (pkt_bytes[4] << 8) | pkt_bytes[5];
            length -= 7;
            pkt_bytes[4] = length >> 8;
            pkt_bytes[5] = length & 0xFF;
            snprintf(serial, max_str_len, "ATLAS RAW %02X%02X%02X%02X%02X%02X%02X[CS]", pkt_bytes[0], pkt_bytes[1], pkt_bytes[2], pkt_bytes[3], pkt_bytes[4], pkt_bytes[5], pkt_bytes[6]);
            delete [] pkt_bytes;

            int term_offset = (int)strlen(serial);
            int parm_num = 0;
            for(int f = 0; f < fields.length(); f++)
            {
                Field* field = fields[f];
                if(field->isPayload())
                {
                    /* Check for Overlay */
                    if((f + 1) < fields.length())
                    {
                        Field* next_field = fields[f+1];
                        if(field->getByteOffset() == next_field->getByteOffset())
                        {
                            if(next_field->getLengthInBits() % 8 == 0)
                            {
                                mlog(WARNING, "Skipping overlayed field: %s\n", field->getName());
                                parm_num++;
                                continue; // current field overlayed by next field
                            }
                        }
                    }

                    /* Write out Field */
                    for(int e = 0; e < field->getNumElements(); e++)
                    {
                        serial[term_offset++] = ' ';
                        if(field->getLengthInBits() % 8 == 0)
                        {
                            for(int b = 0; b < (field->getLengthInBits() / 8); b++)
                            {
                                serial[term_offset++] = Packet::parmSymByte[parm_num % sizeof(Packet::parmSymByte)];
                                serial[term_offset++] = Packet::parmSymByte[parm_num % sizeof(Packet::parmSymByte)];
                            }
                        }
                        else
                        {
                            for(int b = 0; b < field->getLengthInBits(); b++)
                            {
                                serial[term_offset++] = Packet::parmSymBit[parm_num % sizeof(Packet::parmSymBit)];
                            }
                        }
                    }
                    serial[term_offset] = '\0';
                    parm_num++;
                }
            }
        }
    }
    else if(fmt == STOL_CMD_FMT)
    {
        serial = new char[max_str_len];

        /* Build Packet String with STOL */
        snprintf(serial, max_str_len, "/%s ", name);
        for(int f = 0; f < fields.length(); f++)
        {
            Field* field = fields[f];
            if(field->isPayload())
            {
                StringLib::concat(serial, field->getUnqualifiedName(), max_str_len);
                StringLib::concat(serial, "=[", max_str_len);
                StringLib::concat(serial, field->getType(), max_str_len);
                StringLib::concat(serial, "]", max_str_len);

                if((f + 1) < fields.length())
                {
                    StringLib::concat(serial, ", ", max_str_len);
                }
            }
        }
    }
    else if(fmt == READABLE_FMT)
    {
        serial = new char[max_str_len];

        /* Populate Packet String with Actual Values */
        snprintf(serial, max_str_len, "/%s ", name);
        for(int f = 0; f < fields.length(); f++)
        {
            Field* field = fields[f];
            if(field->isPayload())
            {
                StringLib::concat(serial, field->getUnqualifiedName(), max_str_len);
                StringLib::concat(serial, "=", max_str_len);
                if(field->getNumElements() == 1)
                {
                    StringLib::concat(serial, field->getStrValue(0), max_str_len);
                }
                else if(field->isType(Field::STRING))
                {
                    StringLib::concat(serial, "\"", max_str_len);
                    StringLib::concat(serial, field->getStrValue(0), max_str_len);
                    StringLib::concat(serial, "\"", max_str_len);
                }
                else
                {
                    StringLib::concat(serial, "{", max_str_len);
                    for(int e = 0; e < field->getNumElements(); e++)
                    {
                        StringLib::concat(serial, field->getStrValue(e), max_str_len);
                        if((e + 1) < field->getNumElements())
                        {
                            StringLib::concat(serial, ", ", max_str_len);
                        }
                    }
                    StringLib::concat(serial, "}", max_str_len);
                }

                if((f + 1) < fields.length())
                {
                    StringLib::concat(serial, ", ", max_str_len);
                }
            }
        }
    }
    else if(fmt == MULTILINE_FMT)
    {
        serial = new char[max_str_len];

        /* Full Multi-Line Display of Packet Content */
        snprintf(serial, max_str_len, "/%s ", name);
        for(int f = 0; f < fields.length(); f++)
        {
            Field* field = fields[f];
            if(field->isPayload())
            {
                StringLib::concat(serial, field->getUnqualifiedName(), max_str_len);
                StringLib::concat(serial, "=", max_str_len);
                if(field->getNumElements() == 1)
                {
                    StringLib::concat(serial, field->getStrValue(0), max_str_len);
                }
                else if(field->isType(Field::STRING))
                {
                    StringLib::concat(serial, "\"", max_str_len);
                    StringLib::concat(serial, field->getStrValue(0), max_str_len);
                    StringLib::concat(serial, "\"", max_str_len);
                }
                else
                {
                    StringLib::concat(serial, "{", max_str_len);
                    for(int e = 0; e < field->getNumElements(); e++)
                    {
                        StringLib::concat(serial, field->getStrValue(e), max_str_len);
                        if((e + 1) < field->getNumElements())
                        {
                            StringLib::concat(serial, ", ", max_str_len);
                        }
                    }
                    StringLib::concat(serial, "}", max_str_len);
                }
                StringLib::concat(serial, "\n", max_str_len);
            }
        }
    }

    /* Return Serial String */
    return serial;
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *----------------------------------------------------------------------------*/
void Packet::calcAttributes (void)
{
    /* Update Length Field */
    char lenstr[32]; snprintf(lenstr, 32, "0x%04X", numBytes);
    setProperty("length", "defaultValue", lenstr, Field::UNINDEXED_PROP);
}

/*----------------------------------------------------------------------------
 * duplicate  -
 *
 *   Notes: calls virtual _duplicate for each packet type
 *----------------------------------------------------------------------------*/
Packet* Packet::duplicate(void)
{
    Packet* pkt = _duplicate();

    pkt->packetType = packetType;
    pkt->declaration = declaration;
    pkt->numBytes = numBytes;
    pkt->name = StringLib::duplicate(name);
    pkt->currBitOffset = currBitOffset;
    pkt->currByteOffset = currByteOffset;

    int _apid_designation_len = (int)strlen(packetApidDesignation) + 1;
    if(pkt->packetApidDesignation != NULL) delete [] pkt->packetApidDesignation;
    pkt->packetApidDesignation = new char[_apid_designation_len];
    memset(pkt->packetApidDesignation, 0, _apid_designation_len);
    StringLib::copy(pkt->packetApidDesignation, packetApidDesignation, _apid_designation_len);

    for(int f = 0; f < fields.length(); f++)
    {
        Field* _field = fields[f]->duplicate();
        pkt->fields.add(_field);
    }

    pkt->calcAttributes();

    return pkt;
}

/*----------------------------------------------------------------------------
 * populate  -
 *----------------------------------------------------------------------------*/
bool Packet::populate(unsigned char* pkt)
{
    /* Check APID */
    const char* apid_str = getProperty(packetApidDesignation, "value", 0);
    long apid = 0;
    if(!apid_str || !StringLib::str2long(apid_str, &apid))
    {
        mlog(CRITICAL, "Malformed APID property: [%s]\n", apid_str ? apid_str : "NULL");
        return false;
    }
    else if(apid != CCSDS_GET_APID(pkt))
    {
        mlog(WARNING, "Unable to populate packet %s from packet %04X as APIDs do not match! (expected: %04X)\n", name, CCSDS_GET_APID(pkt), (uint16_t)apid);
        return false;
    }

    /* Check Length */
    if(numBytes != CCSDS_GET_LEN(pkt))
    {
        mlog(WARNING, "Unable to populate packet %s from packet %04X as length does not match! (expected: %d, actual: %d)\n", name, CCSDS_GET_APID(pkt), numBytes, CCSDS_GET_LEN(pkt));
        return false;
    }

    /* Populate Fields */
    bool status = true;
    for(int f = 0; f < fields.length(); f++)
    {
        Field* field = fields[f];
        if(field->isPayload())
        {
            if(field->populate(pkt) == false)
            {
                mlog(ERROR, "Unable to populate packet %s with field %s\n", name, field->getName());
                status = false;
            }
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * isName  -
 *----------------------------------------------------------------------------*/
bool Packet::isName(const char* namestr)
{
    return (strcmp(name, namestr) == 0);
}

/*----------------------------------------------------------------------------
 * isType  -
 *----------------------------------------------------------------------------*/
bool Packet::isType(packet_type_t type)
{
    return (type == packetType);
}

/*----------------------------------------------------------------------------
 * isPrototype  -
 *----------------------------------------------------------------------------*/
bool Packet::isPrototype(void)
{
    return declaration->isPrototype();
}

/*----------------------------------------------------------------------------
 * setName  -
 *----------------------------------------------------------------------------*/
void Packet::setName(const char* namestr)
{
    if(name) delete [] name;
    name = StringLib::duplicate(namestr);
}

/*----------------------------------------------------------------------------
 * setDeclaration  -
 *----------------------------------------------------------------------------*/
void Packet::setDeclaration(Record* dec)
{
    declaration = dec;
}

/*----------------------------------------------------------------------------
 * setProperty  -
 *----------------------------------------------------------------------------*/
bool Packet::setProperty(const char* field_name, const char* property_name, const char* value, int index)
{
    for(int i = 0; i < fields.length(); i++)
    {
        Field* field = fields[i];
        if(field->isName(field_name))
        {
            return field->setProperty(property_name, value, index);
        }
    }
    return false;
}

/*----------------------------------------------------------------------------
 * getName  -
 *----------------------------------------------------------------------------*/
const char* Packet::getName(void)
{
    return name;
}

/*----------------------------------------------------------------------------
 * getUndottedName  -
 *----------------------------------------------------------------------------*/
const char* Packet::getUndottedName(void)
{
    return declaration->getUndottedName();
}

/*----------------------------------------------------------------------------
 * getNumBytes  -
 *----------------------------------------------------------------------------*/
int Packet::getNumBytes(void)
{
    return numBytes;
}

/*----------------------------------------------------------------------------
 * getNumFields  -
 *----------------------------------------------------------------------------*/
int Packet::getNumFields(void)
{
    return fields.length();
}

/*----------------------------------------------------------------------------
 * getField  -
 *----------------------------------------------------------------------------*/
Field* Packet::getField(int index)
{
    return fields[index];
}

/*----------------------------------------------------------------------------
 * getField  -
 *----------------------------------------------------------------------------*/
Field* Packet::getField(const char* field_name)
{
    for(int i = 0; i < fields.length(); i++)
    {
        Field* field = fields[i];
        if(field->isName(field_name))
        {
            return field;
        }
    }
    return NULL;
}

/*----------------------------------------------------------------------------
 * getProperty  -
 *----------------------------------------------------------------------------*/
const char* Packet::getProperty(const char* field_name, const char* property_name, int index)
{
    for(int i = 0; i < fields.length(); i++)
    {
        Field* field = fields[i];
        if(field->isName(field_name))
        {
            return field->getProperty(property_name, index);
        }
    }
    return NULL;
}

/*----------------------------------------------------------------------------
 * getApidDesignation  -
 *----------------------------------------------------------------------------*/
const char* Packet::getApidDesignation(void)
{
    return packetApidDesignation;
}

/*----------------------------------------------------------------------------
 * getApid  -
 *----------------------------------------------------------------------------*/
int Packet::getApid(void)
{
    const char* apid_str = getProperty(packetApidDesignation, "defaultValue", 0);
    if(apid_str != NULL)
    {
        long apid = INVALID_APID;
        StringLib::str2long(apid_str, &apid);
        delete [] apid_str;
        if(apid >= 0 && apid < CCSDS_NUM_APIDS)
        {
            return apid;
        }
    }
    return INVALID_APID;
}

/*----------------------------------------------------------------------------
 * getComment  -
 *----------------------------------------------------------------------------*/
const char* Packet::getComment(void)
{
    return declaration->getComment();
}

/*----------------------------------------------------------------------------
 * setPktProperty  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
bool Packet::setPktProperty (const char* property_name, const char* value)
{
    (void)property_name;
    (void)value;

    return false;
}

/*----------------------------------------------------------------------------
 * getPktProperty  -
 *
 *   Notes: virtual
 *----------------------------------------------------------------------------*/
const char* Packet::getPktProperty (const char* property_name)
{
    (void)property_name;

    return NULL;
}

/*----------------------------------------------------------------------------
 * _duplicate  -
 *
 *   Notes: virtual, called via non-virtual duplicate function (this should always be overridden)
 *----------------------------------------------------------------------------*/
Packet* Packet::_duplicate (void)
{
    return NULL;
}

/*----------------------------------------------------------------------------
 * orphanFree  -
 *
 *   Notes: Used as freeing function of a pointer list
 *----------------------------------------------------------------------------*/
void Packet::orphanFree(Record* orphan)
{
    if(orphan)
    {
        int n = orphan->getNumSubRecords();
        for(int i = 0; i < n; i++)
        {
            if(!orphan->isValue()) // values are deallocated when token list deallocated
            {
                orphanFree(orphan->getSubRecord(i));
            }
        }
        delete orphan;
    }
}

/******************************************************************************
 * COMMAND PACKET CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * STATIC DATA
 *----------------------------------------------------------------------------*/
const char* CommandPacket::apidDesignation = RECORD_DEFAULT_APID_DESIGNATION;
const char* CommandPacket::fcDesignation   = RECORD_DEFAULT_FC_DESIGNATION;

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CommandPacket::CommandPacket(command_packet_type_t _type, bool _populate): Packet(COMMAND, _populate, CommandPacket::apidDesignation)
{
    StringLib::copy(criticality, "normal", Record::MAX_TOKEN_SIZE);

    if(_populate)
    {
        setProperty("packetType", "defaultValue", "1", Field::UNINDEXED_PROP);

        if(_type == CommandPacket::ATLAS)
        {
            Record* fc = new Record(false, "U1", fcDesignation);
            Record* cs = new Record(false, "U1", "checksum");

            Field* fc_f = new UnsignedField(fc,  NULL, 0, 1, 8, 48, 6, 0, 0, 0x7F, false, 8, true);
            Field* cs_f = new UnsignedField(cs,     NULL, 0, 1, 8, 56, 7, 0, 0, 0xFF, false, 8, true);

            fields.add(fc_f);
            fields.add(cs_f);

            orphanRecs.add(fc);
            orphanRecs.add(cs);

            numBytes        += 2;
            currBitOffset   += 16;
            currByteOffset  += 2;
        }

        calcAttributes();
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CommandPacket::~CommandPacket(void)
{
}

/*----------------------------------------------------------------------------
 * setPktProperty  -
 *
 *   Notes: virtual parent
 *----------------------------------------------------------------------------*/
bool CommandPacket::setPktProperty (const char* property_name, const char* value)
{
    bool status = true;

    if (strcmp(property_name, "criticality") == 0)
    {
        if(value[0] == '"') StringLib::copy(criticality, value+1, (int)strnlen(value, Record::MAX_TOKEN_SIZE) - 2);
        else                StringLib::copy(criticality, value, Record::MAX_TOKEN_SIZE);
    }
    else if (strcmp(property_name, "criticalityCondition") == 0)
    {
        // do nothing for now
    }
    else
    {
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * getPktProperty  -
 *
 *   Notes:  virtual parent
 *----------------------------------------------------------------------------*/
const char* CommandPacket::getPktProperty (const char* property_name)
{
    if(strcmp(property_name, "criticality") == 0)
    {
        return criticality;
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * _duplicate  -
 *
 *   Notes: virtual, called from non-virtual duplicate function of Packet class
 *----------------------------------------------------------------------------*/
Packet* CommandPacket::_duplicate (void)
{
    Packet* cpkt = new CommandPacket(STANDARD, false);

    cpkt->setPktProperty("criticality", criticality);

    return cpkt;
}

/*----------------------------------------------------------------------------
 * setDesignations  -
 *
 *   Notes: parameters are not copied, they must remain alive
 *----------------------------------------------------------------------------*/
void CommandPacket::setDesignations (const char* _apid_str, const char* _fc_str)
{
    if(_apid_str != NULL)   apidDesignation = StringLib::duplicate(_apid_str);
    else                    apidDesignation = RECORD_DEFAULT_APID_DESIGNATION;

    if(_fc_str != NULL)     fcDesignation = StringLib::duplicate(_fc_str);
    else                    fcDesignation = RECORD_DEFAULT_FC_DESIGNATION;
}

/******************************************************************************
 * TELEMETRY PACKET CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * STATIC DATA
 *----------------------------------------------------------------------------*/
const char* TelemetryPacket::apidDesignation = RECORD_DEFAULT_APID_DESIGNATION;

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TelemetryPacket::TelemetryPacket(telemetry_packet_type_t _type, bool _populate): Packet(TELEMETRY, _populate, TelemetryPacket::apidDesignation)
{
    if(_populate)
    {
        setProperty("packetType", "defaultValue", "0", Field::UNINDEXED_PROP);

        if(_type == ATLAS)
        {
            Record* ts_days = new Record(false, "U12",   "timestamp_days");
            Record* ts_ms = new Record(false, "U1234", "timestamp_ms");

            Field* ts_days_f = new UnsignedField(ts_days,   NULL, 0, 1, 16, 48, 6, 0, 0, 0xFFFF,     false, 16, true);
            Field* ts_ms_f = new UnsignedField(ts_ms,     NULL, 0, 1, 32, 64, 8, 0, 0, 0xFFFFFFFF, false, 32, true);

            fields.add(ts_days_f);
            fields.add(ts_ms_f);

            orphanRecs.add(ts_days);
            orphanRecs.add(ts_ms);

            numBytes        += 6;
            currBitOffset   += 48;
            currByteOffset  += 6;
        }

        calcAttributes();
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TelemetryPacket::~TelemetryPacket(void)
{
    filter = NULL; /* filter is allocated and assigned external to class */
}

/*----------------------------------------------------------------------------
 * setPktProperty  -
 *----------------------------------------------------------------------------*/
bool TelemetryPacket::setPktProperty (const char* property_name, const char* value)
{
    bool status = true;

    if(strcmp(property_name, "applyWhen") == 0)
    {
        SafeString valstr("%s", value);
        applyWhen.add(valstr);
    }
    else if(strcmp(property_name, "timeout") == 0)
    {
        // remove quotations around the timeout value
        char* tvalue = StringLib::duplicate(value);
        int vindex = 0;
        if(tvalue[vindex] == '"')
        {
            vindex++;
            while(tvalue[vindex] != '"' && tvalue[vindex] != '\0') vindex++;
            tvalue[vindex] = '\0';
            vindex = 1;
        }
        if(!StringLib::str2long(&tvalue[vindex], &timeout))
        {
            status = false;
        }
        delete [] tvalue;
    }
    else if(strcmp(property_name, "source") == 0)
    {
        source = value;
    }
    else
    {
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * getPktProperty  -
 *----------------------------------------------------------------------------*/
const char* TelemetryPacket::getPktProperty (const char* property_name)
{
    if (strcmp(property_name, "applyWhen") == 0)
    {
        return NULL;
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * _duplicate  -
 *
 *   Notes: virtual, called from non-virtual duplicate function of Packet class
 *----------------------------------------------------------------------------*/
Packet* TelemetryPacket::_duplicate (void)
{
    Packet* tpkt = new TelemetryPacket(STANDARD, false);

    for(int i = 0; i < applyWhen.length(); i++)
    {
        tpkt->setPktProperty("applyWhen", applyWhen[i].getString());
    }

    return tpkt;
}

/*----------------------------------------------------------------------------
 * setFilter  -
 *----------------------------------------------------------------------------*/
void TelemetryPacket::setFilter (Filter* _filter)
{
    filter = _filter;
}

/*----------------------------------------------------------------------------
 * getFilterProperty  -
 *----------------------------------------------------------------------------*/
const char* TelemetryPacket::getFilterProperty (const char* property_name)
{
    if(filter != NULL)
    {
        return filter->getProperty(property_name);
    }
    else
    {
        return "---";
    }
}

/*----------------------------------------------------------------------------
 * setDesignations  -
 *
 *   Notes: parameters are not copied, they must remain alive
 *----------------------------------------------------------------------------*/
void TelemetryPacket::setDesignations (const char* _apid_str)
{
    if(_apid_str != NULL)   apidDesignation = StringLib::duplicate(_apid_str);
    else                    apidDesignation = RECORD_DEFAULT_APID_DESIGNATION;
}
