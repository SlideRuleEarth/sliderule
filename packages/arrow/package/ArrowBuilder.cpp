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

#include "OsApi.h"
#include "OutputFields.h"
#include "ArrowBuilder.h"
#include "ArrowBuilderImpl.h"
#include "AncillaryFields.h"
#include "ContainerRecord.h"
#include "OutputLib.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowBuilder::OBJECT_TYPE = "ArrowBuilder";
const char* ArrowBuilder::LUA_META_NAME = "ArrowBuilder";
const struct luaL_Reg ArrowBuilder::LUA_META_TABLE[] = {
    {"filenames",   luaGetFileNames},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :parquet(<outq_name>, <inq_name>, <rec_type>, <id>, <parms_str>, <endpoint>, [<keep_local>])
 *----------------------------------------------------------------------------*/
int ArrowBuilder::luaCreate (lua_State* L)
{
    RequestFields* rqst_parms = NULL;

    try
    {
        /* Get Parameters */
        rqst_parms                  = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        const char* outq_name       = getLuaString(L, 2);
        const char* inq_name        = getLuaString(L, 3);
        const char* rec_type        = getLuaString(L, 4);
        const char* id              = getLuaString(L, 5);
        const char* endpoint        = getLuaString(L, 6);
        const bool  keep_local      = getLuaBoolean(L, 7, true, false);

        /* Create Dispatch */
        return createLuaObject(L, new ArrowBuilder(L, rqst_parms, outq_name, inq_name, rec_type, id, endpoint, keep_local));
    }
    catch(const RunTimeException& e)
    {
        if(rqst_parms) rqst_parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * getSubField
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getSubField (const char* field_name)
{
    /* Return Empty String if Null */
    if(!field_name)
    {
        static const char* empty_sub_field = "";
        return empty_sub_field;
    }

    /* Find Last '.' */
    const char* c_ptr = field_name;
    const char* sub_ptr = field_name;
    while(*c_ptr != '\0')
    {
        if(*c_ptr == '.') sub_ptr = c_ptr + 1;
        c_ptr++;
    }

    /* Return Pointer to Subfield */
    return sub_ptr;
}

/*----------------------------------------------------------------------------
 * getDataFile
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getDataFile(void)
{
    return dataFile;
}

/*----------------------------------------------------------------------------
 * getMetadataFile
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getMetadataFile(void)
{
    return metadataFile;
}

/*----------------------------------------------------------------------------
 * getRecType
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getRecType (void)
{
    return recType;
}

/*----------------------------------------------------------------------------
 * getTimeKey
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getTimeKey (void)
{
    return timeKey;
}

/*----------------------------------------------------------------------------
 * getXKey
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getXKey (void)
{
    return xKey;
}

/*----------------------------------------------------------------------------
 * getyKey
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getYKey (void)
{
    return yKey;
}

/*----------------------------------------------------------------------------
 * getXField
 *----------------------------------------------------------------------------*/
RecordObject::field_t& ArrowBuilder::getXField (void)
{
    return geoData.x_field;
}

/*----------------------------------------------------------------------------
 * getYField
 *----------------------------------------------------------------------------*/
RecordObject::field_t& ArrowBuilder::getYField (void)
{
    return geoData.y_field;
}

/*----------------------------------------------------------------------------
 * getParms
 *----------------------------------------------------------------------------*/
const OutputFields* ArrowBuilder::getParms (void)
{
    return &rqstParms->output;
}

/*----------------------------------------------------------------------------
 * hasAncFields
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::hasAncFields (void) const
{
    return hasAncillaryFields;
}

/*----------------------------------------------------------------------------
 * hasAncElements
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::hasAncElements (void) const
{
    return hasAncillaryElements;
}

/*----------------------------------------------------------------------------
 * getParmsString
 *----------------------------------------------------------------------------*/
const string& ArrowBuilder::getParmsAsString (void)
{
    return parmsAsString;
}

/*----------------------------------------------------------------------------
 * getEndpoint
 *----------------------------------------------------------------------------*/
const char* ArrowBuilder::getEndpoint (void)
{
    return endpoint;
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowBuilder::ArrowBuilder (lua_State* L, RequestFields* rqst_parms,
                            const char* outq_name, const char* inq_name,
                            const char* rec_type, const char* id,
                            const char* _endpoint, const bool keep_local):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    rqstParms(rqst_parms),
    parms(rqst_parms->output),
    hasAncillaryFields(false),
    hasAncillaryElements(false),
    parmsAsString(rqstParms->toJson()),
    keepLocal(keep_local)
{
    assert(rqst_parms);
    assert(outq_name);
    assert(inq_name);
    assert(rec_type);
    assert(id);
    assert(_endpoint);

    /* Get Record Meta Data */
    RecordObject::meta_t* rec_meta = RecordObject::getRecordMetaFields(rec_type);
    if(rec_meta == NULL)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to get meta data for %s", rec_type);
    }

    /* Build Geometry Fields */
    if(parms.format == OutputFields::GEOPARQUET)
    {
        /* Check if Record has Geospatial Fields */
        if((rec_meta->x_field == NULL) || (rec_meta->y_field == NULL))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to get x and y coordinates for %s", rec_type);
        }

        /* Get X Field */
        geoData.x_field = RecordObject::getDefinedField(rec_type, rec_meta->x_field);
        if(geoData.x_field.type == RecordObject::INVALID_FIELD)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to extract x field [%s] from record type <%s>", rec_meta->x_field, rec_type);
        }

        /* Get Y Field */
        geoData.y_field = RecordObject::getDefinedField(rec_type, rec_meta->y_field);
        if(geoData.y_field.type == RecordObject::INVALID_FIELD)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to extract y field [%s] from record type <%s>", rec_meta->y_field, rec_type);
        }
    }

    /*
     * NO THROWING BEYOND THIS POINT
     */

    /* Get Paths */
    outputMetadataPath = OutputLib::createMetadataFileName(parms.path.value.c_str());

    /* Save Parameters */
    endpoint = StringLib::duplicate(_endpoint);

    /* Create Unique Temporary Filenames */
    dataFile = OutputLib::getUniqueFileName(id);
    metadataFile = OutputLib::createMetadataFileName(dataFile);

    /* Set Record Type */
    recType = StringLib::duplicate(rec_type);

    /* Save Keys */
    timeKey = StringLib::duplicate(getSubField(rec_meta->time_field));
    xKey = StringLib::duplicate(getSubField(rec_meta->x_field));
    yKey = StringLib::duplicate(getSubField(rec_meta->y_field));

    /* Get Row Size */
    const RecordObject::field_t batch_rec_field = RecordObject::getDefinedField(recType, rec_meta->batch_field);
    if(batch_rec_field.type == RecordObject::INVALID_FIELD) batchRowSizeBytes = 0;
    else batchRowSizeBytes = RecordObject::getRecordDataSize(batch_rec_field.exttype);
    rowSizeBytes = RecordObject::getRecordDataSize(recType) + batchRowSizeBytes;
    maxRowsInGroup = ROW_GROUP_SIZE / rowSizeBytes;

    /* Initialize Queues */
    const int qdepth = maxRowsInGroup * QUEUE_BUFFER_FACTOR;
    outQ = new Publisher(outq_name, Publisher::defaultFree, qdepth);
    inQ = new Subscriber(inq_name, MsgQ::SUBSCRIBER_OF_CONFIDENCE, qdepth);

    /* Allocate Implementation */
    impl = new ArrowBuilderImpl(this);

    /* Start Builder Thread */
    active = true;
    builderPid = new Thread(builderThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ArrowBuilder::~ArrowBuilder(void)
{
    active = false;
    delete builderPid;
    rqstParms->releaseLuaObject();
    delete[] dataFile;
    delete[] metadataFile;
    delete[] outputMetadataPath;
    delete[] endpoint;
    delete[] recType;
    delete[] timeKey;
    delete[] xKey;
    delete[] yKey;
    delete outQ;
    delete inQ;
    delete impl;
}

/*----------------------------------------------------------------------------
 * builderThread
 *----------------------------------------------------------------------------*/
void* ArrowBuilder::builderThread(void* parm)
{
    ArrowBuilder* builder = static_cast<ArrowBuilder*>(parm);
    int row_cnt = 0;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, builder->traceId, "arrow_builder", "{\"filename\":\"%s\"}", builder->dataFile);
    EventLib::stashId(trace_id);

    /* Loop Forever */
    while(builder->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        const int recv_status = builder->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Process Record */
            if(ref.size > 0)
            {
                /* Create Batch Structure */
                RecordInterface* record = new RecordInterface(reinterpret_cast<unsigned char*>(ref.data), ref.size);
                batch_t* batch = new batch_t(ref, builder->inQ);

                /* Process Container Records */
                if(StringLib::match(record->getRecordType(), ContainerRecord::recType))
                {
                    vector<RecordObject*> anc_vec;

                    /* Loop Through Records in Container */
                    ContainerRecord::rec_t* container = reinterpret_cast<ContainerRecord::rec_t*>(record->getRecordData());
                    for(uint32_t i = 0; i < container->rec_cnt; i++)
                    {
                        /* Pull Out Subrecord */
                        const uint8_t* buffer = reinterpret_cast<uint8_t*>(container) + container->entries[i].rec_offset;
                        const int size = container->entries[i].rec_size;
                        RecordObject* subrec = new RecordInterface(buffer, size);

                        /* Handle Supported Record Types */
                        if(StringLib::match(subrec->getRecordType(), builder->recType))
                        {
                            batch->pri_record = subrec;
                        }
                        else if(StringLib::match(subrec->getRecordType(), AncillaryFields::ancFieldArrayRecType))
                        {
                            anc_vec.push_back(subrec);
                            batch->anc_fields += 1;
                        }
                        else if(StringLib::match(subrec->getRecordType(), AncillaryFields::ancElementRecType))
                        {
                            anc_vec.push_back(subrec);
                            const AncillaryFields::element_array_t* element_array = reinterpret_cast<AncillaryFields::element_array_t*>(subrec->getRecordData());
                            batch->anc_elements += element_array->num_elements;
                        }
                        else // ignore
                        {
                            delete subrec; // cleaned up
                        }
                    }

                    /* Clean Up Container Record */
                    delete record;

                    /* Build Ancillary Record Array */
                    batch->num_anc_recs = anc_vec.size();
                    if(batch->num_anc_recs > 0)
                    {
                        batch->anc_records = new RecordObject* [batch->num_anc_recs];
                        for(int i = 0; i < batch->num_anc_recs; i++)
                        {
                            batch->anc_records[i] = anc_vec[i];
                        }
                    }

                    /* Check If Primary Record Found
                     *  must be after above code to populate
                     *  ancillary record array so that they
                     *  get freed */
                    if(!batch->pri_record)
                    {
                        builder->outQ->postCopy(ref.data, ref.size);
                        delete batch;
                        continue;
                    }
                }
                else if(StringLib::match(record->getRecordType(), builder->recType))
                {
                    batch->pri_record = record;
                }
                else
                {
                    /* Record of Non-Targeted Type - Pass Through */
                    builder->outQ->postCopy(ref.data, ref.size);
                    delete record;
                    delete batch;
                    continue;
                }

                /* Determine Rows in Record */
                const int record_size_bytes = batch->pri_record->getAllocatedDataSize();
                const int batch_size_bytes = record_size_bytes - (builder->rowSizeBytes - builder->batchRowSizeBytes);
                batch->rows =  batch_size_bytes / builder->batchRowSizeBytes;

                /* Sanity Check Rows */
                const int left_over = batch_size_bytes % builder->batchRowSizeBytes;
                if(left_over > 0)
                {
                    mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", batch->pri_record->getRecordType(), batch_size_bytes, builder->batchRowSizeBytes);
                    delete batch;
                    continue;
                }

                /* Sanity Check Number of Ancillary Rows */
                if((batch->anc_fields > 0 && batch->anc_fields != batch->rows) ||
                   (batch->anc_elements > 0 && batch->anc_elements != batch->rows))
                {
                    mlog(ERROR, "Attempting to supply ancillary data with mismatched number of rows for %s: %d,%d != %d", batch->pri_record->getRecordType(), batch->anc_fields, batch->anc_elements, batch->rows);
                    delete batch;
                    continue;
                }

                /* Set Ancillary Flags */
                if(batch->anc_fields > 0) builder->hasAncillaryFields = true;
                if(batch->anc_elements > 0) builder->hasAncillaryElements = true;

                /* Add Batch to Ordering */
                builder->recordBatch.add(batch);
                row_cnt += batch->rows;
                if(row_cnt >= builder->maxRowsInGroup)
                {
                    const bool status = builder->impl->processRecordBatch(builder->recordBatch, row_cnt, builder->batchRowSizeBytes * 8);
                    if(!status)
                    {
                        alert(INFO, RTE_FAILURE, builder->outQ, NULL, "Failed to process record batch for %s", builder->parms.path.value.c_str());
                        builder->active = false; // breaks out of loop
                    }
                    builder->recordBatch.clear();
                    row_cnt = 0;
                }
            }
            else
            {
                /* Terminating Message */
                mlog(DEBUG, "Terminator received on %s, exiting parquet builder", builder->inQ->getName());
                builder->active = false; // breaks out of loop
                builder->inQ->dereference(ref); // terminator is not batched, so must dereference here
            }
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d", builder->inQ->getName(), recv_status);
            builder->active = false; // breaks out of loop
        }
    }

    /* Process Remaining Records */
    const bool status = builder->impl->processRecordBatch(builder->recordBatch, row_cnt, builder->batchRowSizeBytes * 8, true);
    if(!status) alert(INFO, RTE_FAILURE, builder->outQ, NULL, "Failed to process last record batch for %s", builder->parms.path.value.c_str());
    builder->recordBatch.clear();

    /* Check if Keeping Local
     *  when performing additional operations on the parquet file, like the ArrowSampler
     *  we need to keep the temporary file on disk so that additional operations can
     *  be performed on it */
    if(!builder->keepLocal)
    {
        /* Send File to User */
        OutputLib::send2User(builder->dataFile, builder->parms.path.value.c_str(), trace_id, &builder->parms, builder->outQ);

        /* Send Metadata File to User */
        if(OutputLib::fileExists(builder->metadataFile))
        {
            OutputLib::send2User(builder->metadataFile, builder->outputMetadataPath, trace_id, &builder->parms, builder->outQ);
        }
    }

    /* Signal Completion */
    builder->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaGetFileNames -
 *----------------------------------------------------------------------------*/
int ArrowBuilder::luaGetFileNames (lua_State* L)
{
    try
    {
        /* Get Self */
        const ArrowBuilder* lua_obj = dynamic_cast<ArrowBuilder*>(getLuaSelf(L, 1));

        /* Return Filenames */
        if(lua_obj->dataFile)       lua_pushstring(L, lua_obj->dataFile);
        else                        lua_pushnil(L);
        if(lua_obj->metadataFile)   lua_pushstring(L, lua_obj->metadataFile);
        else                        lua_pushnil(L);

        /* Success */
        return returnLuaStatus(L, true, 3);
    }
    catch(const RunTimeException& e)
    {
        /* Failure */
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}
