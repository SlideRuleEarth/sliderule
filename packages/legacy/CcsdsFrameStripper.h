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

#ifndef __ccsds_frame_stripper__
#define __ccsds_frame_stripper__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "CcsdsMsgProcessor.h"

/******************************************************************************
 * CCSDS FRAME STRIPPER CLASS
 ******************************************************************************/

class CcsdsFrameStripper: public CcsdsMsgProcessor
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/
        
        typedef enum {
            LSTRIP,
            SYNC,
            FRAME
        } stream_state_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int             LStripSize;
        int             SyncMarkerSize;
        int             FrameFixedSize;
        uint8_t*        SyncMarker;
        
        Publisher*      pubQ;
        stream_state_t  state;
        bool            inSync;
        int             lStripBytes;
        int             syncIndex;
        int             frameIndex;        
        uint8_t*        frameBuffer;
        
        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                CcsdsFrameStripper  (CommandProcessor* cmd_proc, const char* obj_name, const char* inq_name, const char* outq_name, uint8_t* sync_marker, int sync_size, int strip_size, int frame_size);
        virtual ~CcsdsFrameStripper (void);

        bool    processMsg          (unsigned char* msg, int bytes); // OVERLOAD
};

#endif  /* __ccsds_frame_stripper__ */
