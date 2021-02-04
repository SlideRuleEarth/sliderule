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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsParserZFrameModule.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsParserZFrameModule::LuaMetaName = "CcsdsParserZFrameModule";
const struct luaL_Reg CcsdsParserZFrameModule::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create()
 *----------------------------------------------------------------------------*/
int CcsdsParserZFrameModule::luaCreate (lua_State* L)
{
    try
    {
        /* Get File Parameter */
        bool is_file = getLuaBoolean(L, 1);

        /* Return Dispatch Object */
        return createLuaObject(L, new CcsdsParserZFrameModule(L, is_file));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * parseBuffer
 *----------------------------------------------------------------------------*/
int CcsdsParserZFrameModule::parseBuffer (unsigned char* buffer, int bytes, CcsdsPacket* pkt)
{
    unsigned char*  parse_buffer = buffer;
    int             parse_bytes = bytes;
    int             parse_index = 0;

    /* Parse Buffer */
    while(parse_index < parse_bytes)
    {
        /* Determine Number of Bytes left */
        int bytes_left = parse_bytes - parse_index;

        if(state == FRAME_Z)
        {
            /* Synchronization Data for Start of Frame */
            const char* frame_sync = "CCSD3ZA00001";
            #define FRAME_SYNC_SIZE 12 // size of above string

            /* Copy into Frame Buffer */
            int cpylen = MIN(frameZBytes, bytes_left);
            LocalLib::copy(&frameBuffer[frameIndex], &parse_buffer[parse_index], cpylen);
            frameZBytes -= cpylen;
            parse_index += cpylen;
            frameIndex  += cpylen;

            /* Pull off Z header */
            if(frameZBytes == 0)
            {
                /* Compare Sync Mark */
                if(!StringLib::match(frame_sync, frameBuffer, FRAME_SYNC_SIZE))
                {
                    return PARSE_ERROR;
                }
                else
                {
                    /* Pull Out Size */
                    if(!StringLib::str2long(&frameBuffer[12], &frameSize, 10))
                    {
                        mlog(CRITICAL, "Unable to read frame size: %s\n", &frameBuffer[12]);
                        return PARSE_ERROR;
                    }
                    else
                    {
                        /* Set Frame State */
                        frameSize += FRAME_Z_SIZE;
                        frameZBytes = FRAME_Z_SIZE;
                        if(frameFile)
                        {
                            state = FRAME_FILE;
                        }
                        else
                        {
                            state = FRAME_FANN;
                        }
                    }
                }
            }
        }
        else if(state == FRAME_FILE)
        {
            /* Pull off FILE header */
            if(frameFileBytes <= bytes_left)
            {
                parse_index   += frameFileBytes;
                frameIndex    += frameFileBytes;
                frameFileBytes = FRAME_FILE_SIZE;
                state          = FRAME_FANN;
            }
            else
            {
                frameFileBytes -= bytes_left;
                parse_index    += bytes_left;
                frameIndex     += bytes_left;
            }
        }
        else if(state == FRAME_FANN)
        {
            /* Pull off FANN header */
            if(frameFannBytes <= bytes_left)
            {
                parse_index   += frameFannBytes;
                frameIndex    += frameFannBytes;
                frameFannBytes = FRAME_FANN_SIZE;
                state          = FRAME_CXXX;
            }
            else
            {
                frameFannBytes -= bytes_left;
                parse_index    += bytes_left;
                frameIndex     += bytes_left;
            }
        }
        else if(state == FRAME_CXXX)
        {
            /* Pull off CXXX header */
            if(frameCxxBytes <= bytes_left)
            {
                parse_index   += frameCxxBytes;
                frameIndex    += frameCxxBytes;
                frameCxxBytes  = FRAME_CXXX_SIZE;
                state          = CCSDS;
            }
            else
            {
                frameCxxBytes -= bytes_left;
                parse_index   += bytes_left;
                frameIndex    += bytes_left;
            }
        }
        /* Process CCSDS Packet */
        else if(state == CCSDS)
        {
            /* Check that Bytes Left Fit in Frame */
            if((bytes_left + frameIndex) > frameSize)
            {
                bytes_left = frameSize - frameIndex;
            }

            /* Call CCSDS Parser */
            int bytes_parsed = CcsdsParserModule::parseBuffer(&parse_buffer[parse_index], bytes_left, pkt);
            if(bytes_parsed >= 0)
            {
                frameIndex += bytes_parsed;
                parse_index += bytes_parsed;
            }
            else
            {
                return bytes_parsed; // error code
            }

            /* Full Packet Received */
            if(pkt->isFull())
            {
                gotoInitState(false);
                return parse_index;
            }
            else if(bytes_left == 0)
            {
                /* This occurs when the frame check shows we are at the end
                   boundary of a frame and need to go to the next frame to
                   complete the packet */
                gotoInitState(false);
            }
        }
        /* Error in State Machine - Bug in Code */
        else
        {
            assert(false);
        }
    }

    return parse_bytes;
}

/*----------------------------------------------------------------------------
 * gotoInitState
 *----------------------------------------------------------------------------*/
void CcsdsParserZFrameModule::gotoInitState(bool reset)
{
    state = FRAME_Z;
    if(reset)
    {
        /* Initialize Parsing Variables */
        frameZBytes     = FRAME_Z_SIZE;
        frameFileBytes  = FRAME_FILE_SIZE;
        frameFannBytes  = FRAME_FANN_SIZE;
        frameCxxBytes   = FRAME_CXXX_SIZE;
        frameSize       = 0;
        frameIndex      = 0;
        LocalLib::set(frameBuffer, 0, sizeof(frameBuffer));
    }
    else
    {
        /* Check if Frame Complete */
        if(state == FRAME_Z)
        {
            /* Check if Frame is Not Complete - Stay in FANN */
            if(frameIndex < frameSize)
            {
                state = FRAME_FANN;
            }
            /* Check if Frame is Complete - Reset Indices */
            else
            {
                frameIndex = 0;
                frameSize = 0;
            }
        }
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsParserZFrameModule::CcsdsParserZFrameModule(lua_State* L, bool file):
    CcsdsParserModule(L, LuaMetaName, LuaMetaTable)
{
    frameFile = file;
    gotoInitState(true);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsParserZFrameModule::~CcsdsParserZFrameModule(void)
{
}

