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

#ifndef __ccsds_record__
#define __ccsds_record__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "OsApi.h"
#include "RecordObject.h"
#include "CcsdsPacket.h"
#include "Dictionary.h"

/******************************************************************************
 * CCSDS RECORD CLASS
 ******************************************************************************/

class CcsdsRecord: public RecordObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int PKT_KEY_SIZE = 10;
        static const int PKT_CROSS_REF_TBL_SIZE = 0x40000; // 18 bits: 11 bits for APIDs, 7 bits for function codes

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            COMMAND,
            TELEMETRY
        } pktType_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                CcsdsRecord         (const char* populate_string);
                                CcsdsRecord         (unsigned char* buffer, int size);

        /* Overloaded Methods */
        bool                    deserialize         (unsigned char* buffer, int size);
        int                     serialize           (unsigned char** buffer, serialMode_t mode=ALLOCATE);

        /* Regular Methods */
        pktType_t               getPktType          (void);
        uint16_t                getApid             (void);
        uint16_t                getSubtype          (void);
        int                     getSize             (void);

        /* Definition Methods */
        static void             initCcsdsRecord     (void);
        static recordDefErr_t   defineCommand       (const char* rec_type, const char* id_field, uint16_t _apid, uint8_t _fc, int _size, fieldDef_t* fields, int num_fields, int max_fields=MAX_FIELDS);
        static recordDefErr_t   defineTelemetry     (const char* rec_type, const char* id_field, uint16_t _apid, int _size, fieldDef_t* fields, int num_fields, int max_fields=MAX_FIELDS);

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            definition_t*   definition;
            pktType_t       type;       // command or telemetry
            uint16_t        apid;
            uint16_t        subtype;    // overloaded for function code for commands
            int             size;
        } pktDef_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static MgDictionary<pktDef_t*> pktDefs;
        static pktDef_t* pktCrossRefs[PKT_CROSS_REF_TBL_SIZE]; // FC[17:11], APID[10:0]
        static Mutex pktMut;

        pktDef_t* pktDef;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    CcsdsRecord         (void);
        void        populateHeader      (void);
        pktDef_t*   getPacketDefinition (unsigned char* buffer, int size); // overloaded RecordObject method
};

/******************************************************************************
 * CCSDS RECORD INTERFACE CLASS
 ******************************************************************************/

class CcsdsRecordInterface: public CcsdsRecord
{
    public:
                CcsdsRecordInterface     (unsigned char* buffer, int size);
        virtual ~CcsdsRecordInterface    (void);
};

#endif  /* __ccsds_record__ */
