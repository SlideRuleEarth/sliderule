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

#include "CcsdsPayloadDispatch.h"
#include "CcsdsPacket.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPayloadDispatch::LuaMetaName = "CcsdsPayloadDispatch";
const struct luaL_Reg CcsdsPayloadDispatch::LuaMetaTable[] = {
    {"forward",     luaForwardPacket},
    {"checklen",    luaCheckLength},
    {"checkcs",     luaCheckChecksum},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create()
 *----------------------------------------------------------------------------*/
int CcsdsPayloadDispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Return Dispatch Object */
        return createLuaObject(L, new CcsdsPayloadDispatch(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool CcsdsPayloadDispatch::processRecord(RecordObject* record, okey_t key)
{
    (void)key;

    try
    {
        CcsdsSpacePacket ccsds_pkt(record->getRecordData(), record->getRecordDataSize());

        /* Check Packet Length */
        if(checkLength)
        {
            if(record->getRecordDataSize() != ccsds_pkt.getLEN())
            {
                mlog(ERROR, "Incorrect CCSDS packet length detected in %s, dropping packet\n", getName());
                return false;
            }
        }

        /* Check Packet Checksum (if command) */
        if(checkChecksum)
        {
            if(ccsds_pkt.isCMD())
            {
                if(!ccsds_pkt.validChecksum())
                {
                    mlog(ERROR, "Command checksum mismatch detected in %s, dropping packet\n", getName());
                    return false;
                }
            }
        }

        /* Get APID */
        int apid = ccsds_pkt.getAPID();

        /* Post Payload */
        qMut.lock();
        {
            if(outQ[apid])
            {
                int status = outQ[apid]->postCopy(ccsds_pkt.getPayload(), ccsds_pkt.getLEN() - ccsds_pkt.getHdrSize());
                if(status <= 0)
                {
                    mlog(ERROR, "Dropped payload on post to %s with error %d\n", outQ[apid]->getName(), status);
                    return false;
                }
            }
        }
        qMut.unlock();
    }
    catch(std::invalid_argument& e)
    {
        mlog(ERROR, "Unable to create CCSDS packet in %s: %s\n", getName(), e.what());
        return false;
    }

    return true;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsPayloadDispatch::CcsdsPayloadDispatch(lua_State* L):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    LocalLib::set(outQ, 0, sizeof(outQ));
    checkLength = false;
    checkChecksum = false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsPayloadDispatch::~CcsdsPayloadDispatch(void)
{
    for(int i = 0; i < CCSDS_NUM_APIDS; i++)
    {
        setPublisher(i, NULL);
    }
}

/*----------------------------------------------------------------------------
 * setPublisher
 *
 *  Note: assumes apid is valid
 *----------------------------------------------------------------------------*/
void CcsdsPayloadDispatch::setPublisher (int apid, const char* qname)
{
    qMut.lock();
    {
        /* Get Publisher */
        Publisher* pub = NULL;
        if(qname != NULL)
        {
            try
            {
                pub = qLookUp[qname];
            }
            catch(std::out_of_range& e)
            {
                (void)e;
                pub = new Publisher(qname);
                qLookUp.add(qname, pub);
            }
        }

        /* Check Previous Queue */
        if(outQ[apid] != NULL)
        {
            bool still_exists = false;
            const char* prev_qname = outQ[apid]->getName();
            for(int i = 0; i < CCSDS_NUM_APIDS; i++)
            {
                if(outQ[i] != NULL)
                {
                    if(StringLib::match(outQ[i]->getName(), prev_qname))
                    {
                        still_exists = true;
                    }
                }
            }

            /* Delete Publisher if No Longer Needed */
            if(!still_exists)
            {
                qLookUp.remove(prev_qname);
                delete outQ[apid];
            }
        }

        /* Set Publisher */
        outQ[apid] = pub;
    }
    qMut.unlock();
}

/*----------------------------------------------------------------------------
 * luaForwardPacket - :forward(<apid>, <outq name>)
 *----------------------------------------------------------------------------*/
int CcsdsPayloadDispatch::luaForwardPacket(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPayloadDispatch* lua_obj = (CcsdsPayloadDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        long        apid        = getLuaInteger(L, 1);
        const char* outq_name   = getLuaString(L, 2);

        /* Check and Forward APID */
        if(apid >= 0 && apid < CCSDS_NUM_APIDS)
        {
            lua_obj->setPublisher(apid, outq_name);
        }
        else if(apid == ALL_APIDS)
        {
            for(int i = 0; i < CCSDS_NUM_APIDS; i++)
            {
                lua_obj->setPublisher(i, outq_name);
            }
        }
        else
        {
            throw RunTimeException("invalid APID specified: %04X\n", (uint16_t)apid);
        }

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error forwarding packet: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaCheckLength - :checklen(<enable>)
 *----------------------------------------------------------------------------*/
int CcsdsPayloadDispatch::luaCheckLength(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPayloadDispatch* lua_obj = (CcsdsPayloadDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool enable = getLuaBoolean(L, 2);

        /* Set Length Check */
        lua_obj->checkLength = enable;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring length check: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaCheckChecksum - :checkcs(<enable>)
 *----------------------------------------------------------------------------*/
int CcsdsPayloadDispatch::luaCheckChecksum(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPayloadDispatch* lua_obj = (CcsdsPayloadDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool enable = getLuaBoolean(L, 2);

        /* Set Checksum Check */
        lua_obj->checkChecksum = enable;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring checsum check: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
