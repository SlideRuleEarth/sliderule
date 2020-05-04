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

#ifndef __ccsds_parser_zframe_module__
#define __ccsds_parser_zframe_module__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsParserModule.h"
#include "CcsdsPacket.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PARSER ZFRAME MODULE CLASS
 ******************************************************************************/

class CcsdsParserZFrameModule: public CcsdsParserModule
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate       (lua_State* L);
        int         parseBuffer     (unsigned char* buffer, int bytes, CcsdsPacket* pkt); // returns number of bytes consumed
        void        gotoInitState   (bool reset);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const unsigned int FRAME_Z_SIZE      = 20; // Used for socket stream from ADAS
        static const unsigned int FRAME_FILE_SIZE   = 52; // Used for internal ADAS files
        static const unsigned int FRAME_FANN_SIZE   = 58;
        static const unsigned int FRAME_CXXX_SIZE   = 20;

        static const char*  LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            FRAME_Z,
            FRAME_FILE,
            FRAME_FANN,
            FRAME_CXXX,
            CCSDS
        } stream_state_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        stream_state_t  state;
        bool            frameFile;
        int             frameZBytes;
        int             frameFileBytes;
        int             frameFannBytes;
        int             frameCxxBytes;
        long            frameSize;
        int             frameIndex;
        char            frameBuffer[FRAME_Z_SIZE + 1];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        CcsdsParserZFrameModule      (lua_State* L, bool file);
        ~CcsdsParserZFrameModule     (void);
};

#endif  /* __ccsds_parser_zframe_module__ */
