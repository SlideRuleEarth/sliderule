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

#ifndef __record_object__
#define __record_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Dictionary.h"
#include "StringLib.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define TOBYTES(bits)   ((bits) >> 3)
#define TOBITS(bytes)   ((bytes) << 3)


#define RECDEF(type, def, size, key) \
{ \
    RecordObject::recordDefErr_t rc; \
    rc = RecordObject::defineRecord(type, key, size, def, sizeof(def) / sizeof(RecordObject::fieldDef_t)); \
    if(rc != RecordObject::SUCCESS_DEF) \
    { \
        mlog(CRITICAL, "Failed to define %s: %d", type, rc); \
    } \
}

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
            INT8            = 0,
            INT16           = 1,
            INT32           = 2,
            INT64           = 3,
            UINT8           = 4,
            UINT16          = 5,
            UINT32          = 6,
            UINT64          = 7,
            BITFIELD        = 8,
            FLOAT           = 9,
            DOUBLE          = 10,
            TIME8           = 11,   // nanoseconds from Unix epoch, no leap seconds
            STRING          = 12,
            USER            = 13,
            INVALID_FIELD   = 14,
            NUM_FIELD_TYPES = 15
        } fieldType_t;

        typedef enum {
            TEXT,
            REAL,
            INTEGER,
            DYNAMIC
        } valType_t;

        typedef enum {
            COPY,
            ALLOCATE,
            REFERENCE,
            TAKE_OWNERSHIP
        } serialMode_t;

        typedef enum {
            BIGENDIAN       = 0x00000001,
            POINTER         = 0x00000002,
            AUX             = 0x00000004,       // auxiliary field
            BATCH           = 0x00000008,       // batch record
            X_COORD         = 0x00000010,
            Y_COORD         = 0x00000020,
            Z_COORD         = 0x00000040,
            TIME            = 0x00000080,
            INDEX           = 0x00000100
        } fieldFlags_t;

        typedef struct {
            fieldType_t     type;               // predefined types
            int32_t         offset;             // offset in bits into structure
            int32_t         elements;           // number of elements in array
            const char*     exttype;            // record type when type=USER
            unsigned int    flags;              // see fieldFlags_t
        } field_t;

        typedef struct {
            const char*     name;
            fieldType_t     type;
            int32_t         offset;             // bits for BITFIELD, bytes for everything else
            int32_t         elements;
            const char*     exttype;
            uint64_t        flags;              // 64-bits provided here to consume padding
        } fieldDef_t;

        typedef enum {
            SUCCESS_DEF     =  0,
            DUPLICATE_DEF   = -1,
            NOTFOUND_DEF    = -2,
            NUMFIELDERR_DEF = -3,
            FIELDERR_DEF    = -4,
            REGERR_DEF      = -5
        } recordDefErr_t;

        typedef union {
            int8_t      int8_val;
            int16_t     int16_val;
            int32_t     int32_val;
            int64_t     int64_val;
            uint8_t     uint8_val;
            uint16_t    uint16_val;
            uint32_t    uint32_val;
            uint64_t    uint64_val;
            float       float_val;
            double      double_val;
        } type_cast_t;

        typedef struct {
            uint16_t                version;
            uint16_t                type_size;
            uint32_t                data_size;
        } rec_hdr_t;

        struct meta_t
        {
            const char*             index_field;    // field name for index (e.g. extend_id; could be same as id)
            const char*             time_field;     // field name for time
            const char*             x_field;        // field name for x coordinate (e.g. longitude)
            const char*             y_field;        // field name for y coordinate (e.g. latitude)
            const char*             z_field;
            const char*             batch_field;    // field name for batch

            meta_t(void):
                index_field(NULL),
                time_field(NULL),
                x_field(NULL),
                y_field(NULL),
                z_field(NULL),
                batch_field(NULL) {}
            ~meta_t(void)
            {
                delete [] index_field;
                delete [] time_field;
                delete [] x_field;
                delete [] y_field;
                delete [] z_field;
                delete [] batch_field;
            }
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int    RECORD_FORMAT_VERSION = 2;
        static const int    MAX_INITIALIZERS = 64;
        static const int    MAX_VAL_STR_SIZE = 64;
        static const int    CALC_MAX_FIELDS = -1;

        static const char   IMMEDIATE_FIELD_SYMBOL = '$';
        static const char   ARCHITECTURE_TYPE_SYMBOL = '@';

        static const char*  DEFAULT_DOUBLE_FORMAT;
        static const char*  DEFAULT_LONG_FORMAT;
        static const double FLOAT_MAX_VALUE; // maximum 32bit value as a float

        static const int    FIELD_TYPE_BYTES[NUM_FIELD_TYPES];

        /*--------------------------------------------------------------------
         * Field (subclass)
         *--------------------------------------------------------------------*/

        class Field
        {
            public:

                            Field           (RecordObject& _rec, fieldType_t _type, int _offset, int _elements, unsigned int _flags=0, int _element=0);
                            Field           (RecordObject& _rec, const field_t& _field, int _element=0);
                            Field           (const Field& f);
                            ~Field          (void) = default;

                Field&      operator=       (const char* rhs);
                Field&      operator=       (double const& rhs);
                Field&      operator=       (long const& rhs);

                const char* getValueText    (char* valbuf);
                double      getValueReal    (void);
                long        getValueInteger (void);
                valType_t   getValueType    (void);

            private:

                RecordObject&   record;         // parent record
                field_t         field;          // attributes
                int             element;        // for arrays
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit                RecordObject        (const char* rec_type, int allocated_memory=0, bool clear=true); // must include the record type
        explicit                RecordObject        (const unsigned char* buffer, int size);
        virtual                 ~RecordObject       (void);

        /* Overloaded Methods */
        virtual bool            deserialize         (unsigned char* buffer, int size);
        virtual int             serialize           (unsigned char** buffer, serialMode_t mode=ALLOCATE, int size=0);
        bool                    post                (Publisher* outq, int size=0, const bool* active=NULL, bool verbose=true);

        /* Attribute Methods */
        bool                    isRecordType        (const char* rec_type) const;
        const char*             getRecordType       (void) const; // used to identify type of records (used for parsing)
        long                    getRecordId         (void); // used to identify records of the same type (used for filtering))
        unsigned char*          getRecordData       (void) const;
        int                     getRecordTypeSize   (void) const;
        int                     getRecordDataSize   (void) const;
        int                     getAllocatedMemory  (void) const;
        int                     getAllocatedDataSize(void) const;
        int                     getUsedMemory       (void) const;
        int                     getUsedDataSize     (void) const;
        Field*                  createRecordField   (const char* field_name);

        /* Get/Set Methods */
        bool                    populate            (const char* populate_string); // field_name=value, ...
        void                    setIdField          (const char* id_field);
        int                     getNumFields        (void);
        int                     getFieldNames       (char*** names);
        field_t                 getField            (const char* field_name);
        Field                   field               (const char* field_name);
        void                    setValueText        (const field_t& field, const char* val, int element=0);
        void                    setValueReal        (const field_t& field, double val, int element=0);
        void                    setValueInteger     (const field_t& field, long val, int element=0);
        const char*             getValueText        (const field_t& field, char* valbuf=NULL, int element=0);
        double                  getValueReal        (const field_t& field, int element=0);
        long                    getValueInteger     (const field_t& field, int element=0);
        bool                    setUsedData         (int size);

        /* Definition Static Methods */
        static field_t          getDefinedField     (const char* rec_type, const char* field_name);
        static valType_t        getValueType        (const field_t& field);
        static recordDefErr_t   defineRecord        (const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields=CALC_MAX_FIELDS);
        static recordDefErr_t   defineField         (const char* rec_type, const char* field_name, fieldType_t type, int offset, int size, const char* exttype, unsigned int flags=NATIVE_FLAGS);

        /* Utility Static Methods */
        static bool             isRecord            (const char* rec_type);
        static bool             isType              (unsigned char* buffer, int size, const char* rec_type);
        static int              getRecords          (char*** rec_types);
        static const char*      getRecordIdField    (const char* rec_type);
        static const char*      getRecordIndexField (const char* rec_type);
        static const char*      getRecordTimeField  (const char* rec_type);
        static const char*      getRecordXField     (const char* rec_type);
        static const char*      getRecordYField     (const char* rec_type);
        static const char*      getRecordZField     (const char* rec_type);
        static const char*      getRecordBatchField (const char* rec_type);
        static meta_t*          getRecordMetaFields (const char* rec_type);
        static int              getRecordSize       (const char* rec_type);
        static int              getRecordDataSize   (const char* rec_type);
        static int              getRecordMaxFields  (const char* rec_type);
        static int              getRecordFields     (const char* rec_type, char*** field_names, field_t*** fields);
        static Dictionary<field_t>* getRecordFields (const char* rec_type);
        static int              parseSerial         (const unsigned char* buffer, int size, const char** rec_type, const unsigned char** rec_data);
        static unsigned int     str2flags           (const char* str);
        static const char*      flags2str           (unsigned int flags);
        static fieldType_t      str2ft              (const char* str);
        static bool             str2be              (const char* str);
        static const char*      ft2str              (fieldType_t ft);
        static const char*      vt2str              (valType_t vt);
        static unsigned long    unpackBitField      (const unsigned char* buf, int bit_offset, int bit_length);
        static void             packBitField        (unsigned char* buf, int bit_offset, int bit_length, long val);
        static field_t          parseImmediateField (const char* str);

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
            const char*             id_field;       // field name for id; used in dispatches
            meta_t                  meta;           // meta fields populated by the scanDefinition function
            int                     type_size;      // size in bytes of type name string including null termination
            int                     data_size;      // number of bytes of binary data
            int                     record_size;    // total size of memory allocated for record
            Dictionary<field_t>     fields;

            definition_t(const char* _type_name, const char* _id_field, int _data_size, int _max_fields):
                fields(_max_fields)
                { type_name = StringLib::duplicate(_type_name);
                  type_size = StringLib::size(_type_name) + 1;
                  id_field =  StringLib::duplicate(_id_field);
                  data_size = _data_size;
                  record_size = sizeof(rec_hdr_t) + type_size + _data_size; }
            ~definition_t(void)
                { delete [] type_name;
                  delete [] id_field; }
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Dictionary<definition_t*>    definitions;
        static Mutex                        defMut;

        definition_t*   recordDefinition;
        unsigned char*  recordMemory;       // block of allocated memory <record type as null terminated string><record data as binary>
        unsigned char*  recordData;         // pointer to binary data in recordMemory
        int             memoryAllocated;    // number of bytes allocated by object and pointed to by recordMemory
        int             memoryUsed;         // number of bytes used from the memory allocated
        bool            memoryOwner;        // true if object owns (and therefore must free) memory allocated

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        /* Protected Constructor */
                                RecordObject        (void);

        /* Regular Methods */
        field_t                 getPointedToField   (field_t field, bool allow_null, int element=0);
        static field_t          getUserField        (definition_t* def, const char* field_name, uint32_t parent_flags=NATIVE_FLAGS);
        static recordDefErr_t   addDefinition       (definition_t** rec_def, const char* rec_type, const char* id_field, int data_size, const fieldDef_t* fields, int num_fields, int max_fields);
        static recordDefErr_t   addField            (definition_t* def, const char* field_name, fieldType_t type, int offset, int elements, const char* exttype, unsigned int flags);
        static void             scanDefinition      (definition_t* def, const char* field_prefix, const char* rec_type);

        /* Overloaded Methods */
        static definition_t*    getDefinition       (const char* rec_type);
        static definition_t*    getDefinition       (const unsigned char* buffer, int size);
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
                RecordInterface     (const unsigned char* buffer, int size);
                ~RecordInterface    (void) override;
};

#endif  /* __record_object__ */
