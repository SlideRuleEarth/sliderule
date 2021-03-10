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

#ifndef __ccsds_parser_aosframe_module__
#define __ccsds_parser_aosframe_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsParserModule.h"
#include "CcsdsPacket.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PARSER AOS FRAME MODULE CLASS
 ******************************************************************************/

class CcsdsParserAOSFrameModule: public CcsdsParserModule
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate       (lua_State* L);
        int         parseBuffer     (unsigned char* buffer, int bytes, CcsdsPacket* pkt); // returns number of bytes consumed
        void        gotoInitState   (bool reset);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    CcsdsParserAOSFrameModule   (lua_State* L, int scid, int vcid, int strip_size, uint8_t* sync_marker, int sync_size, int sync_offset, int fixed_size, int leading_size, int trailer_size);
        virtual     ~CcsdsParserAOSFrameModule  (void);

        uint16_t    crc16                       (uint8_t* data, uint32_t len, uint16_t crc);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int FRAME_VERSION_NUMBER =  1;
        static const int FRAME_COUNTER_UNSET  = -2;
        static const int FRAME_MPDU_CONTINUE  = 0xFFFF;

        static const char*  LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            LSTRIP,
            SYNC,
            TSTRIP,
            HEADER,
            MPDU,
            CCSDS,
            TRAILER
        } stream_state_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int             LStripSize;
        int             SyncMarkerSize;
        int             TStripSize;
        int             FrameFixedSize;
        int             FrameHeaderSize;
        int             FrameTrailerSize;

        uint8_t*        SyncMarker;
        int             SpacecraftId;
        int             VirtualChannel;
        uint16_t        channelId; // built from SpacecraftId and VirtualChannel

        stream_state_t  state;
        bool            inSync;
        int             lStripBytes;
        int             tStripBytes;
        int             syncIndex;
        int             headerBytes;
        int             trailerBytes;
        int             frameIndex;
        unsigned char*  aosPrimaryHdr; // [FramePrimarySize];
        int             mpduOffset;
        bool            mpduOffsetSet;
        int             frameCounter;
        uint16_t        frameCRC;
        uint8_t*        aosTrailer;
};

#endif  /* __ccsds_parser_aosframe_module__ */
