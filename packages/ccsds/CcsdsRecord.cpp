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

#include "CcsdsRecord.h"
#include "core.h"

/******************************************************************************
 * CCSDS RECORD: STATIC DATA
 ******************************************************************************/

MgDictionary<CcsdsRecord::pktDef_t*> CcsdsRecord::pktDefs;
CcsdsRecord::pktDef_t* CcsdsRecord::pktCrossRefs[PKT_CROSS_REF_TBL_SIZE];
Mutex CcsdsRecord::pktMut;

/******************************************************************************
 * CCSDS RECORD: PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Notes: For CcsdsRecord the record_size is invalid, data_size must be used.  This
 *         is because CcsdsRecords do not prepend the record type string but start
 *         immediately with the CCSDS packet data.
 *----------------------------------------------------------------------------*/
CcsdsRecord::CcsdsRecord(const char* rec_type): RecordObject()
{
    assert(rec_type);

    /* Attempt to Get Record Type */
    recordDefinition = getDefinition(rec_type);

    /* Attempt to Initialize Record */
    if(recordDefinition != NULL)
    {
        memoryAllocated = recordDefinition->data_size;
        recordMemory = new char[memoryAllocated];
        recordData = (unsigned char*)recordMemory;
        memoryOwner = true;
    }
    else
    {
        throw RunTimeException(ERROR, RTE_ERROR, "could not find record definition: %s", rec_type);
    }

    /* Set Packet Definition */
    try
    {
        pktDef = pktDefs[recordDefinition->type_name];
        LocalLib::set(recordMemory, 0, recordDefinition->data_size);
        populateHeader();
    }
    catch(RunTimeException& e)
    {
        (void)e;
        delete [] recordMemory;
        throw RunTimeException(ERROR, RTE_ERROR, "Could not find definition for CCSDS packet with provided record type");
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Notes:  The buffer here (UNLIKE the base class RecordObject) only contains the
 *          binary packet and does not include the type string.
 *----------------------------------------------------------------------------*/
CcsdsRecord::CcsdsRecord(unsigned char* buffer, int size): RecordObject()
{
    pktDef_t* pkt_def = getPacketDefinition(buffer, size);
    if(pkt_def)
    {
        /* Set Record Attributes */
        pktDef = pkt_def;
        recordDefinition = pktDef->definition;
        memoryAllocated = MAX(recordDefinition->data_size, size);
        recordMemory = new char[memoryAllocated];
        recordData = (unsigned char*)recordMemory;
        LocalLib::copy(recordData, buffer, MIN(recordDefinition->data_size, size));
        memoryOwner = true;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Could not convert buffer to valid CCSDS packet");
    }
}

/*----------------------------------------------------------------------------
 * deserialize
 *
 *  Notes:  1. checks that type matches exactly
 *          2. no new memory is allocated
 *          3. only copies the data portion (ccsds packet); type name is left alone
 *----------------------------------------------------------------------------*/
bool CcsdsRecord::deserialize (unsigned char* buffer, int size)
{
    /* Get Record Definition */
    pktDef_t* pkt_def = getPacketDefinition(buffer, size);

    /* Definition Checks  */
    if(pkt_def == NULL)                                 return false;   // could not find definition
    else if(pkt_def->definition != recordDefinition)    return false;   // record does not match definition
    else if(size > memoryAllocated)                     return false;   // buffer passed in too large

    /* Copy in Data */
    LocalLib::copy(recordData, buffer, size);
    return true;
}

/*----------------------------------------------------------------------------
 * serialize
 *
 *  Notes: only returns the data portion (ccsds packet); type name is left alone
 *----------------------------------------------------------------------------*/
int CcsdsRecord::serialize (unsigned char** buffer, serialMode_t mode, int size)
{
    assert(buffer);
    (void)size; // size is a part of the CCSDS packet

    if (mode == ALLOCATE)
    {
        *buffer = new unsigned char[recordDefinition->data_size];
        LocalLib::copy(*buffer, recordData, recordDefinition->data_size);
    }
    else
    {
        *buffer = (unsigned char*)recordData;
    }

    return recordDefinition->data_size;
}

/*----------------------------------------------------------------------------
 * getPktType
 *----------------------------------------------------------------------------*/
CcsdsRecord::pktType_t CcsdsRecord::getPktType(void)
{
    return pktDef->type;
}

/*----------------------------------------------------------------------------
 * getApid
 *----------------------------------------------------------------------------*/
uint16_t CcsdsRecord::getApid(void)
{
    return pktDef->apid;
}

/*----------------------------------------------------------------------------
 * getSubtype
 *----------------------------------------------------------------------------*/
uint16_t CcsdsRecord::getSubtype(void)
{
    return pktDef->subtype;
}

/*----------------------------------------------------------------------------
 * getSize
 *----------------------------------------------------------------------------*/
int CcsdsRecord::getSize(void)
{
    return pktDef->size;
}

/*----------------------------------------------------------------------------
 * initCcsdsRecord
 *----------------------------------------------------------------------------*/
void CcsdsRecord::initCcsdsRecord(void)
{
    for(int i = 0; i < PKT_CROSS_REF_TBL_SIZE; i++)
    {
        pktCrossRefs[i] = NULL;
    }
}

/*----------------------------------------------------------------------------
 * defineCommand
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t CcsdsRecord::defineCommand(const char* rec_type, const char* id_field, uint16_t _apid, uint8_t _fc, int _size, fieldDef_t* fields, int num_fields, int max_fields)
{
    definition_t* rec_def;
    recordDefErr_t status;

    /* Define Underlying Record */
    status = addDefinition(&rec_def, rec_type, id_field, _size, fields, num_fields, max_fields);

    /* Add Packet Definition */
    if(status == SUCCESS_DEF)
    {
        pktMut.lock();
        {
            pktDef_t* pkt_def;
            try
            {
                pkt_def = pktDefs[rec_type];
                status = DUPLICATE_DEF;
            }
            catch(RunTimeException& e)
            {
                (void)e;

                /* Create New Packet Definition */
                assert(_size > 0);
                pkt_def = new pktDef_t;
                pkt_def->definition = rec_def;
                pkt_def->type = CcsdsRecord::COMMAND;
                pkt_def->subtype = _fc;
                pkt_def->apid = _apid;
                pkt_def->size = _size;
                pktDefs.add(rec_type, pkt_def);

                /* Register New Packet Definition */
                unsigned int index = (pkt_def->subtype << 11) | pkt_def->apid;
                pktCrossRefs[index] = pkt_def;
            }
        }
        pktMut.unlock();
    }

    return status;
}

/*----------------------------------------------------------------------------
 * defineTelemetry
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t CcsdsRecord::defineTelemetry(const char* rec_type, const char* id_field, uint16_t _apid, int _size, fieldDef_t* fields, int num_fields, int max_fields)
{
    definition_t* rec_def;
    recordDefErr_t status;

    /* Define Underlying Record */
    status = addDefinition(&rec_def, rec_type, id_field, _size, fields, num_fields, max_fields);

    /* Add Packet Definition */
    if(status == SUCCESS_DEF)
    {
        pktMut.lock();
        {
            pktDef_t* pkt_def;
            try
            {
                pkt_def = pktDefs[rec_type];
                status = DUPLICATE_DEF;
            }
            catch(RunTimeException& e)
            {
                (void)e;

                /* Create New Packet Definition */
                assert(_size > 0);
                pkt_def = new pktDef_t;
                pkt_def->definition = rec_def;
                pkt_def->type = CcsdsRecord::TELEMETRY;
                pkt_def->subtype = 0;
                pkt_def->apid = _apid;
                pkt_def->size = _size;
                pktDefs.add(rec_type, pkt_def);

                /* Register New Packet Definition */
                unsigned int index = (pkt_def->subtype << 11) | pkt_def->apid;
                pktCrossRefs[index] = pkt_def;
            }
        }
        pktMut.unlock();
    }

    return status;
}

/******************************************************************************
 * CCSDS RECORD: PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsRecord::CcsdsRecord(void): RecordObject()
{
}

/*----------------------------------------------------------------------------
 * populateHeader
 *----------------------------------------------------------------------------*/
void CcsdsRecord::populateHeader(void)
{
    CcsdsSpacePacket ccsdspkt(recordData, pktDef->size);
    ccsdspkt.initPkt(pktDef->apid, pktDef->size, false);
    ccsdspkt.setSHDR(true);
    ccsdspkt.setSEQFLG(CcsdsSpacePacket::SEG_NONE);
    if(pktDef->type == COMMAND)
    {
        ccsdspkt.setCMD();
        ccsdspkt.setFunctionCode(pktDef->subtype);
    }
    else
    {
        ccsdspkt.setTLM();
    }
}

/*----------------------------------------------------------------------------
 * getPacketDefinition
 *----------------------------------------------------------------------------*/
CcsdsRecord::pktDef_t* CcsdsRecord::getPacketDefinition(unsigned char* buffer, int size)
{
    if(size < 6) return NULL;

    int len = CCSDS_GET_LEN(buffer);
    if(len > size) return NULL;

    unsigned int apid = CCSDS_GET_APID(buffer);

    unsigned int subtype = 0;
    if(CCSDS_IS_CMD(buffer)) subtype = CCSDS_GET_FC(buffer);

    unsigned int index = (subtype << 11) | apid;
    return pktCrossRefs[index];
}

/******************************************************************************
 * CCSDS RECORD INTERFACE PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsRecordInterface::CcsdsRecordInterface(unsigned char* buffer, int size): CcsdsRecord()
{
    pktDef_t* pkt_def = getPacketDefinition(buffer, size);
    if(pkt_def)
    {
        pktDef = pkt_def;
        recordDefinition = pktDef->definition;
        recordData = buffer;
        recordMemory = (char*)recordData;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Could not create CCSDS record interface using buffer provided");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsRecordInterface::~CcsdsRecordInterface(void)
{

}
