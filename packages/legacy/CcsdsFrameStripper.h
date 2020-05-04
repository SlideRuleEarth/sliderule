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
