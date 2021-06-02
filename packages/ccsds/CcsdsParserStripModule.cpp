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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsParserStripModule.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsParserStripModule::LuaMetaName = "CcsdsParserStripModule";
const struct luaL_Reg CcsdsParserStripModule::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create()
 *----------------------------------------------------------------------------*/
int CcsdsParserStripModule::luaCreate (lua_State* L)
{
    try
    {
        /* Get Header Size Parameter */
        long header_size = getLuaInteger(L, 1);

        /* Return Dispatch Object */
        return createLuaObject(L, new CcsdsParserStripModule(L, header_size));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * parseBuffer
 *----------------------------------------------------------------------------*/
int CcsdsParserStripModule::parseBuffer (unsigned char* buffer, int bytes, CcsdsPacket* pkt)
{
    unsigned char*  parse_buffer = buffer;
    int             parse_bytes = bytes;
    int             parse_index = 0;

    /* Parse Buffer */
    while(parse_index < parse_bytes)
    {
        /* Determine Number of Bytes left */
        int bytes_left = parse_bytes - parse_index;

        /* Pull Off Header */
        if(state == HDR)
        {
            /* Pull off SpaceWire header */
            if(HdrBytes <= bytes_left)
            {
                parse_index += HdrBytes;
                HdrBytes     = HDR_SIZE;
                state        = CCSDS;
            }
            else
            {
                HdrBytes    -= bytes_left;
                parse_index += bytes_left;
            }
        }
        /* Process CCSDS Packet */
        else if(state == CCSDS)
        {
            int parsed_bytes = CcsdsParserModule::parseBuffer(&parse_buffer[parse_index], bytes_left, pkt);
            if(parsed_bytes >= 0)
            {
                parse_index += parsed_bytes;
            }
            else
            {
                return parsed_bytes; // error code
            }
        }
        /* Error in State Machine - Bug in Code */
        else
        {
            assert(false);
        }

        /* Full Packet Received */
        if(pkt->isFull())
        {
            gotoInitState(false);
            break;
        }
    }

    return parse_index;
}

/*----------------------------------------------------------------------------
 * gotoInitState
 *----------------------------------------------------------------------------*/
void CcsdsParserStripModule::gotoInitState(bool reset)
{
    state = HDR;
    if(reset)
    {
        HdrBytes = HDR_SIZE;
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsParserStripModule::CcsdsParserStripModule(lua_State* L, int header_size):
    CcsdsParserModule(L, LuaMetaName, LuaMetaTable),
    HDR_SIZE(header_size)
{
    gotoInitState(true);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsParserStripModule::~CcsdsParserStripModule(void)
{
}
