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
