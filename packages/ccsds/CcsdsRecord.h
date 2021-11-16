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

                                CcsdsRecord         (const char* rec_type);
                                CcsdsRecord         (unsigned char* buffer, int size);

        /* Overloaded Methods */
        bool                    deserialize         (unsigned char* buffer, int size);
        int                     serialize           (unsigned char** buffer, serialMode_t mode=ALLOCATE, int size=0);

        /* Regular Methods */
        pktType_t               getPktType          (void);
        uint16_t                getApid             (void);
        uint16_t                getSubtype          (void);
        int                     getSize             (void);

        /* Definition Methods */
        static void             initCcsdsRecord     (void);
        static recordDefErr_t   defineCommand       (const char* rec_type, const char* id_field, uint16_t _apid, uint8_t _fc, int _size, fieldDef_t* fields, int num_fields, int max_fields=CALC_MAX_FIELDS);
        static recordDefErr_t   defineTelemetry     (const char* rec_type, const char* id_field, uint16_t _apid, int _size, fieldDef_t* fields, int num_fields, int max_fields=CALC_MAX_FIELDS);

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
