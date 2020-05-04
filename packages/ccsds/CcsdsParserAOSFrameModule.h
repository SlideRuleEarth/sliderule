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
