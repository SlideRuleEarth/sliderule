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
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
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
