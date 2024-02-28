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

#include "core.h"
#include "ParquetBuilder.h"
#include "ArrowParms.h"
#include "ArrowImpl.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ParquetBuilder::OBJECT_TYPE = "ParquetBuilder";
const char* ParquetBuilder::LUA_META_NAME = "ParquetBuilder";
const struct luaL_Reg ParquetBuilder::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* ParquetBuilder::metaRecType = "arrowrec.meta";
const RecordObject::fieldDef_t ParquetBuilder::metaRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_meta_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",       RecordObject::INT64,    offsetof(arrow_file_meta_t, size),                      1,  NULL, NATIVE_FLAGS}
};

const char* ParquetBuilder::dataRecType = "arrowrec.data";
const RecordObject::fieldDef_t ParquetBuilder::dataRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_data_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"data",       RecordObject::UINT8,    offsetof(arrow_file_data_t, data),                      0,  NULL, NATIVE_FLAGS} // variable length
};

const char* ParquetBuilder::remoteRecType = "arrowrec.remote";
const RecordObject::fieldDef_t ParquetBuilder::remoteRecDef[] = {
    {"url",   RecordObject::STRING,   offsetof(arrow_file_remote_t, url),                URL_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",  RecordObject::INT64,    offsetof(arrow_file_remote_t, size),                         1,  NULL, NATIVE_FLAGS}
};

const char* ParquetBuilder::TMP_FILE_PREFIX = "/tmp/";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :parquet(<outq_name>, <inq_name>, <rec_type>, <id>, [<x_key>, <y_key>], [<time_key>])
 *----------------------------------------------------------------------------*/
int ParquetBuilder::luaCreate (lua_State* L)
{
    ArrowParms* _parms = NULL;
    geo_data_t geo;
    geo.x_key = NULL;
    geo.y_key = NULL;

    try
    {
        /* Get Parameters */
        _parms                      = dynamic_cast<ArrowParms*>(getLuaObject(L, 1, ArrowParms::OBJECT_TYPE));
        const char* outq_name       = getLuaString(L, 2);
        const char* inq_name        = getLuaString(L, 3);
        const char* rec_type        = getLuaString(L, 4);
        const char* id              = getLuaString(L, 5);
        const char* x_key           = getLuaString(L, 6, true, NULL);
        const char* y_key           = getLuaString(L, 7, true, NULL);
        const char* index_key       = getLuaString(L, 8, true, NULL);

        /* Build Geometry Fields */
        geo.as_geo = _parms->as_geo;
        if(geo.as_geo && (x_key != NULL) && (y_key != NULL))
        {
            geo.x_field = RecordObject::getDefinedField(rec_type, x_key);
            if(geo.x_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract x field [%s] from record type <%s>", x_key, rec_type);
            }

            geo.y_field = RecordObject::getDefinedField(rec_type, y_key);
            if(geo.y_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract y field [%s] from record type <%s>", y_key, rec_type);
            }

            geo.x_key = StringLib::duplicate(x_key);
            geo.y_key = StringLib::duplicate(y_key);
        }
        else
        {
            /* Unable to Create GeoParquet */
            geo.as_geo = false;
        }

        /* Create Dispatch */
        return createLuaObject(L, new ParquetBuilder(L, _parms, outq_name, inq_name, rec_type, id, geo, index_key));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        delete [] geo.x_key;
        delete [] geo.y_key;
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ParquetBuilder::init (void)
{
    RECDEF(metaRecType, metaRecDef, sizeof(arrow_file_meta_t), NULL);
    RECDEF(dataRecType, dataRecDef, sizeof(arrow_file_data_t), NULL);
    RECDEF(remoteRecType, remoteRecDef, sizeof(arrow_file_remote_t), NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ParquetBuilder::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * getFileName
 *----------------------------------------------------------------------------*/
const char* ParquetBuilder::getFileName (void)
{
    return fileName;
}

/*----------------------------------------------------------------------------
 * getRecType
 *----------------------------------------------------------------------------*/
const char* ParquetBuilder::getRecType (void)
{
    return recType;
}

/*----------------------------------------------------------------------------
 * getIndexKey
 *----------------------------------------------------------------------------*/
const char* ParquetBuilder::getIndexKey (void)
{
    return indexKey;
}
/*----------------------------------------------------------------------------

 * getAsGeo
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::getAsGeo (void)
{
    return geoData.as_geo;
}

/*----------------------------------------------------------------------------
 * getXField
 *----------------------------------------------------------------------------*/
RecordObject::field_t& ParquetBuilder::getXField (void)
{
    return geoData.x_field;
}

/*----------------------------------------------------------------------------
 * getYField
 *----------------------------------------------------------------------------*/
RecordObject::field_t& ParquetBuilder::getYField (void)
{
    return geoData.y_field;
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ParquetBuilder::ParquetBuilder (lua_State* L, ArrowParms* _parms,
                                const char* outq_name, const char* inq_name,
                                const char* rec_type, const char* id, const geo_data_t& geo, const char* index_key):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    recType(StringLib::duplicate(rec_type)),
    geoData(geo)
{
    assert(_parms);
    assert(outq_name);
    assert(inq_name);
    assert(rec_type);
    assert(id);

    /* Get Path */
    if(parms->asset_name)
    {
        /* Check Private Cluster */
        if(OsApi::getIsPublic())
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to stage output on public cluster");
        }

        /* Generate Output Path */
        Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(parms->asset_name, Asset::OBJECT_TYPE));
        const char* path_prefix = StringLib::match(asset->getDriver(), "s3") ? "s3://" : "";
        const char* path_suffix = parms->as_geo ? ".geoparquet" : ".parquet";
        FString path_name("/%s.%016lX", id, OsApi::time(OsApi::CPU_CLK));
        FString path_str("%s%s%s%s", path_prefix, asset->getPath(), path_name.c_str(), path_suffix);
        asset->releaseLuaObject();

        /* Set Output Path */
        outputPath = path_str.c_str(true);
        mlog(INFO, "Generating unique path: %s", outputPath);
    }
    else if((parms->path == NULL) || (parms->path[0] == '\0'))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to determine output path");
    }
    else
    {
        outputPath = StringLib::duplicate(parms->path);
    }

    /* Create Unique Temporary Filename */
    FString tmp_file("%s%s.parquet", TMP_FILE_PREFIX, id);
    fileName = tmp_file.c_str(true);

    /* Save Index Key */
    indexKey = StringLib::duplicate(index_key);

    /* Allocate Implementation */
    impl = new ArrowImpl(this);

    /* Row Based Parameters */
    batchRowSizeBytes = RecordObject::getRecordDataSize(impl->getBatchRecType());
    rowSizeBytes = RecordObject::getRecordDataSize(recType) + batchRowSizeBytes;
    maxRowsInGroup = ROW_GROUP_SIZE / rowSizeBytes;

    /* Initialize Queues */
    int qdepth = maxRowsInGroup * QUEUE_BUFFER_FACTOR;
    outQ = new Publisher(outq_name, Publisher::defaultFree, qdepth);
    inQ = new Subscriber(inq_name, MsgQ::SUBSCRIBER_OF_CONFIDENCE, qdepth);

    /* Start Builder Thread */
    if(impl->isValid())
    {
        active = true;
        builderPid = new Thread(builderThread, this);
    }
    else
    {
        active = false;
        builderPid = NULL;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ParquetBuilder::~ParquetBuilder(void)
{
    active = false;
    delete builderPid;
    parms->releaseLuaObject();
    delete [] fileName;
    delete [] outputPath;
    delete [] recType;
    delete [] indexKey;
    delete outQ;
    delete inQ;
    delete impl;
    if(geoData.as_geo)
    {
        delete [] geoData.x_key;
        delete [] geoData.y_key;
    }
}

/*----------------------------------------------------------------------------
 * builderThread
 *----------------------------------------------------------------------------*/
void* ParquetBuilder::builderThread(void* parm)
{
    ParquetBuilder* builder = static_cast<ParquetBuilder*>(parm);
    int row_cnt = 0;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, builder->traceId, "parquet_builder", "{\"filename\":\"%s\"}", builder->fileName);
    EventLib::stashId(trace_id);

    /* Loop Forever */
    while(builder->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        int recv_status = builder->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Process Record */
            if(ref.size > 0)
            {
                /* Get Record and Match to Type being Processed */
                RecordInterface* record = new RecordInterface((unsigned char*)ref.data, ref.size);
                if(!StringLib::match(record->getRecordType(), builder->recType))
                {
                    delete record;
                    builder->outQ->postCopy(ref.data, ref.size);
                    builder->inQ->dereference(ref);
                    continue;
                }

                /* Determine Rows in Record */
                int record_size_bytes = record->getAllocatedDataSize();
                int batch_size_bytes = record_size_bytes - (builder->rowSizeBytes - builder->batchRowSizeBytes);
                int num_rows =  batch_size_bytes / builder->batchRowSizeBytes;
                int left_over = batch_size_bytes % builder->batchRowSizeBytes;
                if(left_over > 0)
                {
                    mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", record->getRecordType(), batch_size_bytes, builder->batchRowSizeBytes);
                    delete record; // record is not batched, so must delete here
                    builder->inQ->dereference(ref); // record is not batched, so must dereference here
                    continue;
                }

                /* Create Batch Structure */
                batch_t batch = {
                    .ref = ref,
                    .record = record,
                    .rows = num_rows
                };

                /* Add Batch to Ordering */
                builder->recordBatch.add(row_cnt, batch);
                row_cnt += num_rows;
                if(row_cnt >= builder->maxRowsInGroup)
                {
                    bool status = builder->impl->processRecordBatch(builder->recordBatch, row_cnt, builder->batchRowSizeBytes * 8, builder->geoData);
                    if(!status)
                    {
                        alert(RTE_ERROR, INFO, builder->outQ, NULL, "Failed to process record batch for %s", builder->outputPath);
                        builder->active = false; // breaks out of loop
                    }
                    builder->clearBatch(trace_id);
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
    bool status = builder->impl->processRecordBatch(builder->recordBatch, row_cnt, builder->batchRowSizeBytes * 8, builder->geoData, true);
    if(!status) alert(RTE_ERROR, INFO, builder->outQ, NULL, "Failed to process last record batch for %s", builder->outputPath);
    builder->clearBatch(trace_id);

    /* Send File to User */
    const char* _path = builder->outputPath;
    uint32_t send_trace_id = start_trace(INFO, trace_id, "send_file", "{\"path\": \"%s\"}", _path);
    int _path_len = StringLib::size(_path);
    if( (_path_len > 5) &&
        (_path[0] == 's') &&
        (_path[1] == '3') &&
        (_path[2] == ':') &&
        (_path[3] == '/') &&
        (_path[4] == '/'))
    {
        /* Upload File to S3 */
        builder->send2S3(&_path[5]);
    }
    else
    {
        /* Stream File Back to Client */
        builder->send2Client();
    }

    /* Remove File */
    int rc = remove(builder->fileName);
    if(rc != 0)
    {
        mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, builder->fileName, strerror(errno));
    }

    stop_trace(INFO, send_trace_id);

    /* Signal Completion */
    builder->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * clearBatch
 *----------------------------------------------------------------------------*/
void ParquetBuilder::clearBatch (uint32_t trace_id)
{
    batch_t batch;
    uint32_t clear_trace_id = start_trace(INFO, trace_id, "clear_batch", "%s", "{}");
    unsigned long key = recordBatch.first(&batch);
    while(key != (unsigned long)INVALID_KEY)
    {
        delete batch.record;
        inQ->dereference(batch.ref);
        key = recordBatch.next(&batch);
    }
    recordBatch.clear();
    stop_trace(INFO, clear_trace_id);
}

/*----------------------------------------------------------------------------
 * send2S3
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::send2S3 (const char* s3dst)
{
    #ifdef __aws__

    bool status = true;

    /* Check Path */
    if(!s3dst) return false;

    /* Get Bucket and Key */
    char* bucket = StringLib::duplicate(s3dst);
    char* key = bucket;
    while(*key != '\0' && *key != '/') key++;
    if(*key == '/')
    {
        *key = '\0';
    }
    else
    {
        status = false;
        mlog(CRITICAL, "invalid S3 url: %s", s3dst);
    }
    key++;

    /* Put File */
    if(status)
    {
        /* Send Initial Status */
        alert(RTE_INFO, INFO, outQ, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        try
        {
            /* Upload to S3 */
            int64_t bytes_uploaded = S3CurlIODriver::put(fileName, bucket, key, parms->region, &parms->credentials);

            /* Send Successful Status */
            alert(RTE_INFO, INFO, outQ, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);

            /* Send Remote Record */
            RecordObject remote_record(remoteRecType);
            arrow_file_remote_t* remote = (arrow_file_remote_t*)remote_record.getRecordData();
            StringLib::copy(&remote->url[0], outputPath, URL_MAX_LEN);
            remote->size = bytes_uploaded;
            if(!remote_record.post(outQ))
            {
                mlog(CRITICAL, "Failed to send remote record back to user for %s", outputPath);
            }
        }
        catch(const RunTimeException& e)
        {
            status = false;

            /* Send Error Status */
            alert(RTE_ERROR, e.level(), outQ, NULL, "Upload to S3 failed, bucket = %s, key = %s, error = %s", bucket, key, e.what());
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    alert(RTE_ERROR, CRITICAL, outQ, NULL, "Output path specifies S3, but server compiled without AWS support");
    return false;
    #endif
}

/*----------------------------------------------------------------------------
 * send2Client
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::send2Client (void)
{
    bool status = true;

    /* Reopen Parquet File to Stream Back as Response */
    FILE* fp = fopen(fileName, "r");
    if(fp)
    {
        /* Get Size of File */
        fseek(fp, 0L, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        /* Log Status */
        mlog(INFO, "Writing parquet file %s of size %ld", fileName, file_size);

        do
        {
            /* Send Meta Record */
            RecordObject meta_record(metaRecType);
            arrow_file_meta_t* meta = (arrow_file_meta_t*)meta_record.getRecordData();
            StringLib::copy(&meta->filename[0], parms->path, FILE_NAME_MAX_LEN);
            meta->size = file_size;
            if(!meta_record.post(outQ))
            {
                status = false;
                break; // early exit on error
            }

            /* Send Data Records */
            long offset = 0;
            while(offset < file_size)
            {
                RecordObject data_record(dataRecType, 0, false);
                arrow_file_data_t* data = (arrow_file_data_t*)data_record.getRecordData();
                StringLib::copy(&data->filename[0], parms->path, FILE_NAME_MAX_LEN);
                size_t bytes_read = fread(data->data, 1, FILE_BUFFER_RSPS_SIZE, fp);
                if(!data_record.post(outQ, offsetof(arrow_file_data_t, data) + bytes_read))
                {
                    status = false;
                    break; // early exit on error
                }
                offset += bytes_read;
            }
        } while(false);

        /* Close File */
        int rc = fclose(fp);
        if(rc != 0)
        {
            status = false;
            mlog(CRITICAL, "Failed (%d) to close file %s: %s", rc, fileName, strerror(errno));
        }
    }
    else // unable to open file
    {
        status = false;
        mlog(CRITICAL, "Failed (%d) to read parquet file %s: %s", errno, fileName, strerror(errno));
    }

    /* Return Status */
    return status;
}
