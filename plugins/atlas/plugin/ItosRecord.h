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

#ifndef __itos_record__
#define __itos_record__

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/*************************************************
 * DEFINES
 *************************************************/

#define RECORD_DEFAULT_APID_DESIGNATION "applicationId"
#define RECORD_DEFAULT_FC_DESIGNATION   "functionCode"

namespace Itos
{

    /*************************************************
     * CLASS: Record
     *************************************************/
    class Record
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            Record              (bool _is_prototype, const char* _type, const char* _name);
                            ~Record             (void);

            void            addSubRecord        (Record* record);
            void            addValue            (const char* value);

            bool            isValue             (void);
            bool            isRedefinition      (void);
            bool            isType              (const char* typestr);
            bool            isPrototype         (void);

            void            setPrototype        (bool _prototype);
            void            setComment          (const char* _comment);

            int             getNumSubRecords    (void);
            int             getNumSubValues     (void);
            Record*         getSubRecord        (int index);
            const char*     getSubValue         (int index);
            const char*     getName             (void);
            const char*     getUnqualifiedName  (void);
            const char*     getDisplayName      (void);
            const char*     getUndottedName     (void);
            int             getNumArrayElements (void);
            const char*     getType             (void);
            const char*     getComment          (void);

            /* -------------------------- */
            /* Constants                  */
            /* -------------------------- */

            static const int MAX_TOKEN_SIZE = 1024;
            static const int MAX_VAL_SIZE   = 32;

      private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            bool                prototype;
            const char*         type;
            const char*         name;
            List<Record*>       subrecords;
            List<const char*>   subvalues;
            const char*         comment;
    };

    /*************************************************
     * CLASS: CmdEnumeration
     *************************************************/

    class TypeConversion
    {
        public:

            /* -------------------------- */
            /* Typedefs                   */
            /* -------------------------- */

            typedef enum {
                CMD_ENUM,
                TLM_CONV,
                EXP_ALGO,
                EXP_CONV,
                PLY_CONV,
                UKNOWN
            } type_conv_t;

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                        TypeConversion  (type_conv_t _type, const char* _name);
                        ~TypeConversion (void);

            void        addEnumLookup   (const char* conv_name, const char* value);
            const char* getEnumValue    (const char* conv_name);
            const char* getName         (void);
            int         getNames        (char*** names);
            const char* getType         (void);
            bool        isName          (const char* _name);
            const char* getAsHtml       (bool comma_separate=true);

            /* -------------------------- */
            /* Constants                  */
            /* -------------------------- */

            static  const int   MAX_STR_LEN = 4096;

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            type_conv_t             type;
            const char*             name;
            Dictionary<const char*> lookup;
    };

    /*************************************************
     * CLASS: Field
     *************************************************/

    class Field
    {
        public:

            /* -------------------------- */
            /* Typedefs                   */
            /* -------------------------- */

            typedef enum {
                INTEGER,
                UNSIGNED,
                FLOAT,
                STRING,
            } field_type_t;

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                                    Field           (   field_type_t    _field_type,
                                                        Record*         _record,
                                                        Record*         _container,
                                                        int             _container_index,
                                                        int             _num_elements,
                                                        int             _length_in_bits,
                                                        int             _offset_in_bits,
                                                        int             _byte_offset,
                                                        bool            _payload,
                                                        int             _base_size_in_bits,
                                                        bool            _big_endian );
            virtual                 ~Field          (void);

                    bool            setProperty         (const char* property, const char* _value, int index);
                    const char*     getProperty         (const char* property, int index=0);

                    const char*     getDisplayName      (char* buf); // populates buf and returns pointer to buf
                    const char*     getUnqualifiedName  (void);
                    const char*     getUndottedName     (void);
                    const char*     getName             (void);
                    const char*     getType             (void);
                    int             getOffsetInBits     (void);
                    int             getByteOffset       (void);
                    int             getLengthInBits     (void);
                    int             getNumElements      (void);
                    int             getByteSize         (void);
                    int             getBaseSizeInBits   (void);
                    unsigned long   getBitMask          (void);
                    const char*     getConversion       (void);
                    bool            getBigEndian        (void);
                    const char*     getComment          (void);

                    bool            isName              (const char* namestr);
                    bool            isPayload           (void);
                    bool            isType              (field_type_t type);

            virtual Field*          duplicate           (void);
            virtual unsigned long   getRawValue         (int element);
            virtual const char*     getStrValue         (int element);
            virtual bool            _setProperty        (const char* property, const char* _value, int index);
            virtual const char*     _getProperty        (const char* property, int index);
            virtual bool            populate            (unsigned char* pkt);

            /* -------------------------- */
            /* Constants                  */
            /* -------------------------- */

            static  const int       UNINDEXED_PROP  = 0;

        protected:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            Record*         record;
            Record*         container;
            int             containerIndex;
            field_type_t    fieldType;
            int             numElements;    // number of elements represented by this field (for arrays))
            int             lengthInBits;   // number of bits of the individual field element
            int             offsetInBits;   // number of bits in packet to the start of the first field element
            int             byteOffset;     // number of bytes to the start of the first field element
            unsigned long   bitMask;        // bits in packet that are used by field - calculated
            int             byteSize;       // number of bytes touched in packet by field - calculated
            bool            payload;        // a payload field is a non-ccsds header field
            int             baseSizeInBits; // set via record type
            bool            bigEndian;      // set via record type
            const char*     conversion;
            bool            rangeChecking;  // defaults to on

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

            void            calcAttributes  (void);
    };

    /*************************************************
     * SUBCLASS: IntegerField
     *************************************************/

    class IntegerField: public Field
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            IntegerField    (   Record* _record,
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
                                                bool    _big_endian );
                            ~IntegerField   (void);

            Field*          duplicate       (void);
            unsigned long   getRawValue     (int element);
            const char*     getStrValue     (int element);
            bool            _setProperty    (const char* property, const char* _value, int index);
            const char*     _getProperty    (const char* property, int index);
            bool            populate        (unsigned char* pkt);

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            long*           value;
            long            minRange;
            long            maxRange;
    };

    /*************************************************
     * SUBCLASS: UnsignedField
     *************************************************/

    class UnsignedField: public Field
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            UnsignedField   (   Record*         _record,
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
                                                bool            _big_endian );
                            ~UnsignedField  (void);

            Field*          duplicate       (void);
            unsigned long   getRawValue     (int element);
            const char*     getStrValue     (int element);
            bool            _setProperty    (const char* property, const char* _value, int index);
            const char*     _getProperty    (const char* property, int index);
            bool            populate        (unsigned char* pkt);

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            unsigned long*  value;
            unsigned long   minRange;
            unsigned long   maxRange;
    };

    /*************************************************
     * SUBCLASS: FloatField
     *************************************************/

    class FloatField: public Field
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            FloatField      (   Record* _record,
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
                                                bool    _big_endian );
                            ~FloatField     (void);

            Field*          duplicate       (void);
            unsigned long   getRawValue     (int element);
            const char*     getStrValue     (int element);
            bool            _setProperty    (const char* property, const char* _value, int index);
            const char*     _getProperty    (const char* property, int index);
            bool            populate        (unsigned char* pkt);

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            double*         value;
            double          minRange;
            double          maxRange;
    };

    /*************************************************
     * SUBCLASS: StringField
     *************************************************/

    class StringField: public Field
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            StringField     (   Record*     _record,
                                                Record*     _container,
                                                int         _container_index,
                                                int         _num_elements,
                                                int         _length_in_bits,
                                                int         _offset_in_bits,
                                                int         _byte_offset,
                                                const char* _default_value,
                                                bool        _payload,
                                                int         _base_size_in_bits,
                                                bool        _big_endian );
                            ~StringField    (void);

            Field*          duplicate       (void);
            unsigned long   getRawValue     (int element);
            const char*     getStrValue     (int element);
            bool            _setProperty    (const char* property, const char* _value, int index);
            const char*     _getProperty    (const char* property, int index);
            bool            populate        (unsigned char* pkt);

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            char value[Record::MAX_TOKEN_SIZE];
    };

    /*************************************************
     * CLASS: Filter
     *************************************************/

    class Filter
    {
        public:

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                        Filter          (   int          _q,
                                            int          _spw,
                                            const char*  _fsw_define,
                                            int          _sid,
                                            double       _rate,
                                            const char*  _type,
                                            const char*  _sender,
                                            const char*  _task,
                                            const char** _sources);
                        ~Filter         (void);

            const char* getProperty     (const char* name);
            bool        onApid          (int apid);

            /* -------------------------- */
            /* Constants                  */
            /* -------------------------- */

            static  const int   MAX_STR_LEN = 128;

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            int                 q;
            int                 spw;
            char                fsw_define[MAX_STR_LEN];
            int                 sid;
            double              rate;
            char                type[MAX_STR_LEN];
            char                sender[MAX_STR_LEN];
            char                task[MAX_STR_LEN];
            List<SafeString>    source;
    };

    /*************************************************
     * CLASS: Packet
     *************************************************/

    class Packet
    {
        public:

            /* -------------------------- */
            /* Typedefs                   */
            /* -------------------------- */

            typedef enum {
                COMMAND,
                TELEMETRY
            } packet_type_t;

            typedef enum {
                RAW_STOL_CMD_FMT,
                STOL_CMD_FMT,
                READABLE_FMT,
                MULTILINE_FMT,
                BINARY_FMT
            } serialization_format_t;

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                                Packet              (packet_type_t _packet_type, bool populate=true, const char* _apid_designation=RECORD_DEFAULT_APID_DESIGNATION);
                    virtual     ~Packet             (void);

                    void        addField            (   Record*             record,
                                                        Record*             container,
                                                        int                 container_index,
                                                        Field::field_type_t type,
                                                        int                 size_in_bits,
                                                        bool                big_endian);
                    char*       serialize           (serialization_format_t fmt,  int max_str_len);
                    void        calcAttributes      (void);
                    Packet*     duplicate           (void);
                    bool        populate            (unsigned char* pkt);

                    bool        isName              (const char* namestr);
                    bool        isType              (packet_type_t type);
                    bool        isPrototype         (void);

                    void        setName             (const char* namestr);
                    void        setDeclaration      (Record* dec);
                    bool        setProperty         (const char* field_name, const char* property_name, const char* value, int index);

                    const char* getName             (void);
                    const char* getUndottedName     (void);
                    int         getNumBytes         (void);
                    int         getNumFields        (void);
                    Field*      getField            (int index);
                    Field*      getField            (const char* field_name);
                    const char* getProperty         (const char* field_name, const char* property_name, int index);

                    const char* getApidDesignation  (void);
                    int         getApid             (void);

                    const char* getComment          (void);

            virtual bool        setPktProperty      (const char* property_name, const char* value); // for setting packet properties
            virtual const char* getPktProperty      (const char* property_name);                    // for getting packet properties
            virtual Packet*     _duplicate          (void);

                    void        orphanFree          (Record* orphan);

            /* -------------------------- */
            /* Constants                  */
            /* -------------------------- */

            static const int    INVALID_APID = -1;
            static const int    NumParmSyms = 20;
            static const char   parmSymByte[NumParmSyms];
            static const char   parmSymBit[NumParmSyms];

        protected:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            packet_type_t           packetType;
            Record*                 declaration;        // allocated externally
            List<Record*>           orphanRecs;         // records that need to be cleaned up if packet is deleted
            MgList<Field*>          fields;
            int                     numBytes;
            char*                   name;               // allocated locally

            int                     currBitOffset;      // true bit offset of the field
            int                     currByteOffset;     // starting byte of the field (for bit fields it is the starting byte of the field which could span multiple bytes)

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            char* packetApidDesignation;
    };

    /*************************************************
     * SUBCLASS: CommandPacket
     *************************************************/

    class CommandPacket: public Packet
    {
        public:

            /* -------------------------- */
            /* Typedefs                   */
            /* -------------------------- */

            typedef enum {
                STANDARD,
                ATLAS,
            } command_packet_type_t;

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            CommandPacket       (command_packet_type_t _type, bool populate=true);
                            ~CommandPacket      (void);

            bool            setPktProperty      (const char* property_name, const char* value); // for setting packet properties
            const char*     getPktProperty      (const char* property_name);                    // for getting packet properties
            Packet*         _duplicate          (void);

            static void     setDesignations     (const char* _apid_str, const char* _fc_str);

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            static const char*  apidDesignation;    // Application ID Designation
            static const char*  fcDesignation;      // Function Code Designation

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            char criticality[Record::MAX_TOKEN_SIZE];
    };

    /*************************************************
     * SUBCLASS: TelemetryPacket
     *************************************************/

    class TelemetryPacket: public Packet
    {
        public:

            /* -------------------------- */
            /* Typedefs                   */
            /* -------------------------- */

            typedef enum {
                STANDARD,
                ATLAS
            } telemetry_packet_type_t;

            /* -------------------------- */
            /* Methods                    */
            /* -------------------------- */

                            TelemetryPacket     (telemetry_packet_type_t _type = STANDARD, bool populate=true);
                            ~TelemetryPacket    (void);

            bool            setPktProperty      (const char* property_name, const char* value); // for setting packet properties
            const char*     getPktProperty      (const char* property_name);                    // for getting packet properties
            Packet*         _duplicate          (void);
            void            setFilter           (Filter* _filter);
            const char*     getFilterProperty   (const char* property_name);

            static void     setDesignations    (const char* _apid_str);

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            static const char*  apidDesignation;    // Application ID Designation

        private:

            /* -------------------------- */
            /* Data                       */
            /* -------------------------- */

            List<SafeString>    applyWhen;
            Filter*             filter;
            long                timeout;
            SafeString          source;
    };

    /*************************************************
     * SUBCLASS: TelemetryPacket
     *************************************************/

    class Mnemonic
    {
        public:

            const char* name;
            const char* type;
            const char* source;
            const char* source_packet;
            const char* initial_value;
            TypeConversion* conversion;
    };

} /* namespace Itos */

#endif  /* __itos_record__ */
