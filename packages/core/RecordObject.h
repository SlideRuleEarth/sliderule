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

#ifndef __record_object__
#define __record_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Dictionary.h"
#include "StringLib.h"
#include "OsApi.h"

#include <exception>
#include <stdexcept>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define TOBYTES(bits)   ((bits) >> 3)
#define TOBITS(bytes)   ((bytes) << 3)

#ifdef __LE__
#define NATIVE_FLAGS 0
#else
#define NATIVE_FLAGS 1
#endif

/******************************************************************************
 * RECORD EXCEPTION CLASSES
 ******************************************************************************/

class InvalidRecordException : public std::runtime_error
{
   public:
       InvalidRecordException(const char* _errmsg="InvalidRecordException") : std::runtime_error(_errmsg) { }
};

class AccessRecordException : public std::runtime_error
{
   public:
       AccessRecordException(const char* _errmsg="AccessRecordException") : std::runtime_error(_errmsg) { }
};

/******************************************************************************
 * RECORD OBJECT CLASS
 ******************************************************************************/

class RecordObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            INT8,
            INT16,
            INT32,
            INT64,
            UINT8,
            UINT16,
            UINT32,
            UINT64,
            BITFIELD,
            FLOAT,
            DOUBLE,
            TIME8,
            STRING,
            INVALID_FIELD
        } fieldType_t;

        typedef enum {
            TEXT,
            REAL,
            INTEGER,
            INVALID_VALUE
        } valType_t;

        typedef enum {
            COPY,
            REPLACE
        } deserialMode_t;

        typedef enum {
            ALLOCATE,
            REFERENCE
        } serialMode_t;

        typedef enum {
            BIGENDIAN       = 0x00000001,
            POINTER         = 0x00000002
        } fieldFlags_t;

        typedef struct {
            fieldType_t     type;               // predefined types
            int32_t         size;               // size in bits of field
            int32_t         offset;             // offset in bits into structure
            unsigned int    flags;              // see fieldFlags_t
        } field_t;

        typedef struct {
            const char*     name;
            fieldType_t     type;
            int32_t         offset;             // bits for BITFIELD, bytes for everything else
            int32_t         size;               // bits for BITFIELD, bytes for everything else
            unsigned int    flags;
        } fieldDef_t;

        typedef enum {
            SUCCESS_DEF     =  0,
            DUPLICATE_DEF   = -1,
            NOTFOUND_DEF    = -2,
            NUMFIELDERR_DEF = -3,
            FIELDERR_DEF    = -4
        } recordDefErr_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_CVT_NAME_SIZE = 1024;
        static const int MAX_INITIALIZERS = 64;
        static const int MAX_VAL_STR_SIZE = 64;
        static const int MAX_FIELDS = 256;

        static const char   IMMEDIATE_FIELD_SYMBOL = '$';
        static const char   ARCHITECTURE_TYPE_SYMBOL = '@';

        static const char*  DEFAULT_DOUBLE_FORMAT;
        static const char*  DEFAULT_LONG_FORMAT;
        static const double FLOAT_MAX_VALUE; // maximum 32bit value as a float

        /*--------------------------------------------------------------------
         * Field (subclass)
         *--------------------------------------------------------------------*/

        class Field
        {
            public:

                            Field           (RecordObject& _rec, fieldType_t _type, int _offset, int _size, unsigned int _flags=0);
                            Field           (RecordObject& _rec, field_t _field);
                            Field           (const Field& f);
                            ~Field          (void);

                Field&      operator=       (const char* const rhs);
                Field&      operator=       (double const& rhs);
                Field&      operator=       (long const& rhs);

                const char* getValueText    (char* valbuf);
                double      getValueReal    (void);
                long        getValueInteger (void);
                valType_t   getValueType    (void);

            private:

                RecordObject&   record;         // parent record
                field_t         field;          // attributes
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                RecordObject        (const char* populate_string, int allocated_memory=0); // must include the record type
                                RecordObject        (unsigned char* buffer, int size);
        virtual                 ~RecordObject       (void);

        /* Overloaded Methods */
        virtual bool            deserialize         (unsigned char* buffer, int size);
        virtual int             serialize           (unsigned char** buffer, serialMode_t mode=ALLOCATE);

        /* Attribute Methods */
        bool                    isRecordType        (const char* rec_type);
        const char*             getRecordType       (void); // used to identify type of records (used for parsing)
        long                    getRecordId         (void); // used to identify records of the same type (used for filtering))
        unsigned char*          getRecordData       (void);
        int                     getRecordTypeSize   (void);
        int                     getRecordDataSize   (void);
        int                     getAllocatedMemory  (void);
        Field*                  createRecordField   (const char* field_name);

        /* Get/Set Methods */
        void                    setIdField          (const char* id_field);
        int                     getNumFields        (void);
        int                     getFieldNames       (char*** names);
        field_t                 getField            (const char* field_name);
        Field                   field               (const char* field_name);
        void                    setValueText        (field_t field, const char* val);
        void                    setValueReal        (field_t field, const double val);
        void                    setValueInteger     (field_t field, const long val);
        const char*             getValueText        (field_t field, char* valbuf=NULL);
        double                  getValueReal        (field_t field);
        long                    getValueInteger     (field_t field);
        valType_t               getValueType        (field_t field);

        /* Definition Static Methods */
        static recordDefErr_t   defineRecord        (const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields=MAX_FIELDS);
        static recordDefErr_t   defineField         (const char* rec_type, const char* field_name, fieldType_t type, int offset, int size, unsigned int flags=NATIVE_FLAGS);

        /* Utility Static Methods */
        static bool             isRecord            (const char* rec_type);
        static bool             isType              (unsigned char* buffer, int size, const char* rec_type);
        static int              getRecords          (char*** rec_types);
        static const char*      getRecordIdField    (const char* rec_type); // returns name of field
        static int              getRecordSize       (const char* rec_type);
        static int              getRecordDataSize   (const char* rec_type);
        static int              getRecordMaxFields  (const char* rec_type);
        static int              getRecordFields     (const char* rec_type, char*** field_names, field_t*** fields);
        static int              parseSerial         (unsigned char* buffer, int size, const char** rec_type, const unsigned char** rec_data);
        static unsigned int     str2flags           (const char* str);
        static const char*      flags2str           (unsigned int flags);
        static fieldType_t      str2ft              (const char* str);
        static bool             str2be              (const char* str);
        static const char*      ft2str              (fieldType_t ft);
        static const char*      vt2str              (valType_t vt);
        static unsigned long    unpackBitField      (unsigned char* buf, int bit_offset, int bit_length);
        static void             packBitField        (unsigned char* buf, int bit_offset, int bit_length, long val);
        static field_t          parseImmediateField (const char* str);
        static const char*      buildRecType        (const char* rec_type_str, char* rec_type_buf, int buf_size);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int FIELD_HASH_SCALAR = 2;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        struct definition_t
        {
            const char*             type_name;      // the name of the type of record
            const char*             id_field;       // field name for id
            int                     type_size;      // size in bytes of type name string including null termination
            int                     data_size;      // number of bytes of binary data
            int                     record_size;    // total size of memory allocated for record
            Dictionary<field_t>     fields;

            definition_t(const char* _type_name, const char* _id_field, int _data_size, int _max_fields):
                fields(_max_fields)
                { type_name = StringLib::duplicate(_type_name);
                  type_size = (int)StringLib::size(_type_name) + 1;
                  id_field = StringLib::duplicate(_id_field);
                  data_size = _data_size;
                  record_size = type_size + _data_size; }
            ~definition_t(void)
                { if(type_name) delete [] type_name;
                  if(id_field) delete [] id_field; }
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static MgDictionary<definition_t*>  definitions;
        static Mutex                        defMut;

        definition_t*   recordDefinition;
        char*           recordMemory;       // block of allocated memory <record type as null terminated string><record data as binary>
        unsigned char*  recordData;         // pointer to binary data in recordMemory
        int             memoryAllocated;    // number of bytes allocated by object and pointed to by recordMemory

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        /* Protected Constructor */
                                RecordObject        (void);

        /* Regular Methods */
        bool                    populate            (const char* populate_string); // field_name=value, ...
        field_t                 getPointedToField   (field_t field, bool allow_null);
        static recordDefErr_t   addDefinition       (definition_t** rec_def, const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields);
        static recordDefErr_t   addField            (definition_t* def, const char* field_name, fieldType_t type, int offset, int size, unsigned int flags=NATIVE_FLAGS);

        /* Overloaded Methods */
        static definition_t*    getDefinition       (const char* rec_type);
        static definition_t*    getDefinition       (unsigned char* buffer, int size);
};

/*----------------------------------------------------------------------------
 * Exported Typedef (Syntax Sugar)
 *----------------------------------------------------------------------------*/

typedef RecordObject::Field RecordField;

/******************************************************************************
 * RECORD INTERFACE CLASS
 ******************************************************************************/

class RecordInterface: public RecordObject
{
    public:
                RecordInterface     (unsigned char* buffer, int size);
        virtual ~RecordInterface    (void);
};

#endif  /* __record_object__ */
