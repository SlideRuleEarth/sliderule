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

#ifndef __ccsds_packet__
#define __ccsds_packet__

/* CCSDS Space Packet Primary Header
 *
 *       Stream ID
 *        bits  shift   ------------ description ----------------
 *       0x07FF    0  : application ID
 *       0x0800   11  : secondary header: 0 = absent, 1 = present
 *       0x1000   12  : packet type:      0 = TLM, 1 = CMD
 *       0xE000   13  : CCSDS version, always set to 0
 *
 *       Sequence Count
 *        bits  shift   ------------ description ----------------
 *       0x3FFF    0  : sequence count
 *       0xC000   14  : segmentation flags:  3 = complete packet
 *
 *       Length
 *        bits  shift   ------------ description ----------------
 *       0xFFFF    0  : (total packet length) - 7
 */

/* CCSDS Telecommand Packet Secondary Header
 *
 *        bits  shift   ------------ description ----------------
 *       0x00FF    0  : checksum, calculated by ground system
 *       0x7F00    8  : command function code
 *       0x8000   15  : reserved, set to 0
 */

/* CCSDS Telemetry Packet Secondary Header
 *
 *        bits  shift   ------------ description ----------------
 *       0xFFFF    0  : days since GPS epoch (Jan 6, 1980)
 *       0xFFFF    0  : MSBs milliseconds in the current day
 *       0xFFFF   16  : LSBs milliseconds in the current day
 */

/*
 * CCSDS Encapsulation Packet Header
 *
 * PACKET      PROTOCOL    LENGTH      USER            PROTOCOL        CCSDS           PACKET
 * VERSION     ID          OF          DEFINED         ID              DEFINED         LENGTH
 * NUMBER                  LENGTH      FIELD           EXTENSION       FIELD
 * 3 bits      3 bits      2 bits      0 or 4 bits     0 or 4 bits     0 or 2 bytes    0 to 4 bytes
 * --------    --------    --------    --------        --------        --------        --------
 * ‘111’       ‘XXX’       ‘00’        0 bits          0 bits          0 octets        0 octets
 * ‘111’       ‘XXX’       ‘01’        0 bits          0 bits          0 octets        1 octet
 * ‘111’       ‘XXX’       ‘10’        4 bits          4 bits          0 octets        2 octets
 * ‘111’       ‘XXX’       ‘11’        4 bits          4 bits          2 octets        4 octets
 *
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "TimeLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define CCSDS_MAX_SPACE_PACKET_SIZE     0x10006
#define CCSDS_SPACE_HEADER_SIZE         6
#define CCSDS_MAX_ENCAP_PACKET_SIZE     0x40000
#define CCSDS_ENCAP_HEADER_SIZE         1 // minimum (maximum is 8)
#define CCSDS_NUM_APIDS                 2048
#define CCSDS_NUM_FCS                   128

#define CCSDS_ENCAP_PROTO_IDLE          0
#define CCSDS_ENCAP_PROTO_LTP           1
#define CCSDS_ENCAP_PROTO_IPE           2
#define CCSDS_ENCAP_PROTO_CFDP          3
#define CCSDS_ENCAP_PROTO_BP            4
#define CCSDS_ENCAP_PROTO_EXTENSION     6
#define CCSDS_ENCAP_PROTO_PRIVATE       7

#ifndef CCSDS_FC_OFFSET                 // support for placing command function code elsewhere in packet
#define CCSDS_FC_OFFSET                 6
#endif

#ifndef CCSDS_CS_OFFSET                 // support for placing command checksum elsewhere in packet
#define CCSDS_CS_OFFSET                 7
#endif

#define CCSDS_GET_SID(buffer)           ((buffer[0] << 8) + buffer[1])
#define CCSDS_GET_APID(buffer)          (((buffer[0] << 8) + buffer[1]) & 0x07FF)
#define CCSDS_HAS_SHDR(buffer)          ((buffer[0] & 0x08) == 0x08)
#define CCSDS_IS_CMD(buffer)            ((buffer[0] & 0x10) == 0x10)
#define CCSDS_IS_TLM(buffer)            ((buffer[0] & 0x10) != 0x10)
#define CCSDS_GET_SEQ(buffer)           (((buffer[2] & 0x3F) << 8) + buffer[3])
#define CCSDS_GET_SEQFLG(buffer)        ((CcsdsSpacePacket::seg_flags_t)(buffer[2] & 0xC0))
#define CCSDS_GET_LEN(buffer)           ((buffer[4] << 8) + (buffer[5] + 7))
#define CCSDS_GET_FC(buffer)            (buffer[CCSDS_FC_OFFSET] & 0x7f)
#define CCSDS_GET_CS(buffer)            (buffer[CCSDS_CS_OFFSET])
#define CCSDS_GET_CDS_DAYS(buffer)      ((buffer[6] << 8) + buffer[7])
#define CCSDS_GET_CDS_MSECS(buffer)     ((buffer[8] << 24) + (buffer[9] << 16) + (buffer[10] << 8) + buffer[11])

#define ALL_APIDS                       CCSDS_NUM_APIDS

/******************************************************************************
 * CCSDS PACKET CLASS
 ******************************************************************************/

class CcsdsPacket
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int CCSDS_ERROR            = -1;
        static const int CCSDS_LEN_ERROR        = -2;
        static const int CCSDS_PKT_ERROR        = -3;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            INVALID_PACKET = 0,
            ENCAPSULATION_PACKET = 1,
            SPACE_PACKET = 2
        } type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit                CcsdsPacket         (type_t _type);
        virtual                 ~CcsdsPacket        (void);
        type_t                  getType             (void);

        virtual int             getAPID             (void) const = 0;
        virtual void            setAPID             (int apid) = 0;
        virtual int             getSEQ              (void) const = 0;
        virtual void            setSEQ              (int value) = 0;
        virtual int             getLEN              (void) const = 0;
        virtual void            setLEN              (int value) = 0;

        virtual void            initPkt             (int apid, int len, bool clear) = 0;
        virtual void            resetPkt            (void) = 0;

        virtual bool            setIndex            (int offset) = 0;
        virtual int             getIndex            (void) const = 0;
        virtual int             appendStream        (unsigned char* bytes, int len) = 0;
        virtual bool            isFull              (void) const = 0;

        virtual unsigned char*  getBuffer           (void) = 0;
        virtual unsigned char*  getPayload          (void) = 0;
        virtual int             getHdrSize          (void) const = 0;

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        unsigned char*          buffer;
        int                     index;
        bool                    is_malloced;
        int                     max_pkt_len;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        type_t                  pkt_type;
};

/******************************************************************************
 * CCSDS SPACE PACKET CLASS
 ******************************************************************************/

class CcsdsSpacePacket: public CcsdsPacket
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            SEG_START       = 0x40,
            SEG_CONTINUE    = 0x00,
            SEG_STOP        = 0x80,
            SEG_NONE        = 0xC0,
            SEG_ERROR       = 0xFF
        } seg_flags_t;

        typedef TimeLib::gmt_time_t pkt_time_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int CCSDS_SECHDR_OFFSET    = 6;
        static const int CCSDS_CMDPAY_OFFSET    = 8;
        static const int CCSDS_TLMPAY_OFFSET    = 12;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

       explicit             CcsdsSpacePacket    (int len=CCSDS_MAX_SPACE_PACKET_SIZE);
                            CcsdsSpacePacket    (uint16_t apid, int len, bool clear);
                            CcsdsSpacePacket    (unsigned char* buf, int size, bool copy=false);
                            ~CcsdsSpacePacket   (void) override;

        int                 getAPID             (void) const override;
        void                setAPID             (int apid) override;
        bool                hasSHDR             (void) const;
        void                setSHDR             (bool value);
        bool                isCMD               (void) const;
        void                setCMD              (void);
        bool                isTLM               (void) const;
        void                setTLM              (void);
        uint8_t             getVERS             (void) const;
        void                setVERS             (uint8_t value);
        int                 getSEQ              (void) const override;
        void                setSEQ              (int value) override;
        seg_flags_t         getSEQFLG           (void) const;
        void                setSEQFLG           (seg_flags_t value);
        int                 getLEN              (void) const override;
        void                setLEN              (int value) override;

        int                 getFunctionCode     (void) const;
        bool                setFunctionCode     (uint8_t value);
        int                 getChecksum         (void) const;
        bool                setChecksum         (uint8_t value);
        int                 getCdsDays          (void) const;
        bool                setCdsDays          (uint16_t days);
        long                getCdsMsecs         (void) const;
        bool                setCdsMsecs         (uint32_t msecs);
        double              getCdsTime          (void) const;
        pkt_time_t          getCdsTimeAsGmt     (void) const;
        bool                setCdsTime          (double gps);

        void                initPkt             (int apid, int len, bool clear) override;
        void                resetPkt            (void) override;
        bool                loadChecksum        (void);
        bool                validChecksum       (void) const;
        int                 computeChecksum     (void) const;

        bool                setIndex            (int offset) override;
        int                 getIndex            (void) const override;
        int                 appendStream        (unsigned char* bytes, int len) override;
        bool                isFull              (void) const override;

        unsigned char*      getBuffer           (void) override;
        unsigned char*      getPayload          (void) override;
        int                 getHdrSize          (void) const override;

        CcsdsSpacePacket&   operator=           (const CcsdsSpacePacket& rhp);

        static const char*  seg2str             (seg_flags_t seg);
};

/******************************************************************************
 * CCSDS ENCAPSULATION PACKET CLASS
 ******************************************************************************/

class CcsdsEncapPacket: public CcsdsPacket
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit        CcsdsEncapPacket    (int len=CCSDS_MAX_SPACE_PACKET_SIZE);
                        ~CcsdsEncapPacket   (void) override;

        int             getAPID             (void) const override;
        void            setAPID             (int apid) override;
        int             getSEQ              (void) const override;
        void            setSEQ              (int value) override;
        int             getLEN              (void) const override;
        void            setLEN              (int value) override;

        void            initPkt             (int apid, int len, bool clear) override;
        void            resetPkt            (void) override;

        bool            setIndex            (int offset) override;
        int             getIndex            (void) const override;
        int             appendStream        (unsigned char* bytes, int len) override;
        bool            isFull              (void) const override;

        unsigned char*  getBuffer           (void) override;
        unsigned char*  getPayload          (void) override;
        int             getHdrSize          (void) const override;
};

#endif /* __ccsds_packet__*/
