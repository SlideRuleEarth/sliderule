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

#include <filesystem>
#include <uuid/uuid.h>

#include "OsApi.h"
#include "RequestFields.h"
#include "OutputLib.h"
#include "RecordObject.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

static const char* TMP_FILE_PREFIX = "/tmp/";

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* OutputLib::metaRecType   = "arrowrec.meta";
const char* OutputLib::dataRecType   = "arrowrec.data";
const char* OutputLib::eofRecType    = "arrowrec.eof";
const char* OutputLib::remoteRecType = "arrowrec.remote";

const RecordObject::fieldDef_t OutputLib::metaRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(output_file_meta_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",       RecordObject::INT64,    offsetof(output_file_meta_t, size),                      1,  NULL, NATIVE_FLAGS}
};

const RecordObject::fieldDef_t OutputLib::dataRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(output_file_data_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"data",       RecordObject::UINT8,    offsetof(output_file_data_t, data),                      0,  NULL, NATIVE_FLAGS} // variable length
};

const RecordObject::fieldDef_t OutputLib::eofRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(output_file_eof_t, filename),   FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"checksum",   RecordObject::UINT64,   offsetof(output_file_eof_t, checksum),                   1,  NULL, NATIVE_FLAGS}
};

const RecordObject::fieldDef_t OutputLib::remoteRecDef[] = {
    {"url",   RecordObject::STRING,   offsetof(output_file_remote_t, url),                URL_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",  RecordObject::INT64,    offsetof(output_file_remote_t, size),                         1,  NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void OutputLib::init(void)
{
    RECDEF(metaRecType, metaRecDef, sizeof(output_file_meta_t), NULL);
    RECDEF(dataRecType, dataRecDef, sizeof(output_file_data_t), NULL);
    RECDEF(eofRecType, eofRecDef, sizeof(output_file_eof_t), NULL);
    RECDEF(remoteRecType, remoteRecDef, sizeof(output_file_remote_t), NULL);
}

/*----------------------------------------------------------------------------
 * send2User
 *----------------------------------------------------------------------------*/
bool OutputLib::send2User (const char* fileName, const char* outputPath,
                uint32_t traceId, const OutputFields* parms, Publisher* outQ)
{
    bool status = false;

    /* Send File to User */
    const char* _path = outputPath;
    const uint32_t send_trace_id = start_trace(INFO, traceId, "send_file", "{\"path\": \"%s\"}", _path);
    const int _path_len = StringLib::size(_path);
    if( (_path_len > 5) &&
        (_path[0] == 's') &&
        (_path[1] == '3') &&
        (_path[2] == ':') &&
        (_path[3] == '/') &&
        (_path[4] == '/'))
    {
        /* Upload File to S3 */
        status = send2S3(fileName, &_path[5], outputPath, parms, outQ);
    }
    else if( (_path_len > 7) &&
             (_path[0] == 'f') &&
             (_path[1] == 'i') &&
             (_path[2] == 'l') &&
             (_path[3] == 'e') &&
             (_path[4] == ':') &&
             (_path[5] == '/') &&
             (_path[6] == '/'))
    {
        /* Rename File - very fast if both files are on the same partition */
        renameFile(fileName, &_path[7]);
        status = true;
    }
    else
    {
        /* Stream File Back to Client */
        status = send2Client(fileName, outputPath, parms, outQ);
    }

    /* Delete File Locally */
    removeFile(fileName);

    stop_trace(INFO, send_trace_id);
    return status;
}

/*----------------------------------------------------------------------------
 * send2S3
 *----------------------------------------------------------------------------*/
bool OutputLib::send2S3 (const char* fileName, const char* s3dst, const char* outputPath,
              const OutputFields* parms, Publisher* outQ)
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
        alert(INFO, RTE_STATUS, outQ, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        /* Upload to S3 */
        int attempt = 0;
        int64_t bytes_uploaded = 0;
        while(bytes_uploaded == 0 && attempt++ < S3CurlIODriver::ATTEMPTS_PER_REQUEST)
        {
            try
            {
                bytes_uploaded = S3CurlIODriver::put(fileName, bucket, key, parms->region.value.c_str(), &parms->credentials);
            }
            catch(const RunTimeException& e)
            {
                alert(e.level(), RTE_FAILURE, outQ, NULL, "S3 PUT failed attempt %d, bucket = %s, key = %s, error = %s", attempt, bucket, key, e.what());
            }
        }

        if(bytes_uploaded > 0)
        {
            /* Send Successful Status */
            alert(INFO, RTE_STATUS, outQ, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);

            /* Send Remote Record */
            RecordObject remote_record(remoteRecType);
            output_file_remote_t* remote = reinterpret_cast<output_file_remote_t*>(remote_record.getRecordData());
            StringLib::copy(&remote->url[0], outputPath, URL_MAX_LEN);
            remote->size = bytes_uploaded;
            if(!remote_record.post(outQ))
            {
                mlog(CRITICAL, "Failed to send remote record back to user for %s", outputPath);
            }
        }
        else
        {
            /* Set Error Status */
            status = false;

            /* Send Error Status */
            alert(CRITICAL, RTE_FAILURE, outQ, NULL, "Upload to S3 failed, bucket = %s, key = %s", bucket, key);
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    alert(CRITICAL, RTE_FAILURE, outQ, NULL, "Output path specifies S3, but server compiled without AWS support");
    return false;
    #endif
}

/*----------------------------------------------------------------------------
 * send2Client
 *----------------------------------------------------------------------------*/
bool OutputLib::send2Client (const char* fileName, const char* outPath, const OutputFields* parms, Publisher* outQ)
{
    bool status = true;

    /* Reopen File to Stream Back as Response */
    FILE* fp = fopen(fileName, "r");
    if(fp)
    {
        /* Get Size of File */
        fseek(fp, 0L, SEEK_END);
        const long file_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        /* Log Status */
        mlog(INFO, "Sending file %s of size %ld to %s", fileName, file_size, outPath);

        do
        {
            uint64_t checksum = 0;

            /* Send Meta Record */
            RecordObject meta_record(metaRecType);
            output_file_meta_t* meta = reinterpret_cast<output_file_meta_t*>(meta_record.getRecordData());
            StringLib::copy(&meta->filename[0], outPath, FILE_NAME_MAX_LEN);
            meta->size = file_size;
            if(!meta_record.post(outQ))
            {
                status = false;
                mlog(CRITICAL, "Failed to post meta record for file %s", fileName);
                break; // early exit on error
            }

            /* Send Data Records */
            long offset = 0;
            while(offset < file_size)
            {
                const long bytes_left_to_send = file_size - offset;
                const long bytes_to_send = MIN(bytes_left_to_send, FILE_BUFFER_RSPS_SIZE);
                const long record_bytes = offsetof(output_file_data_t, data) + bytes_to_send;
                RecordObject data_record(dataRecType, record_bytes, false);
                output_file_data_t* data = reinterpret_cast<output_file_data_t*>(data_record.getRecordData());
                StringLib::copy(&data->filename[0], outPath, FILE_NAME_MAX_LEN);
                const size_t bytes_read = fread(data->data, 1, bytes_to_send, fp);
                if(!data_record.post(outQ, offsetof(output_file_data_t, data) + bytes_read))
                {
                    status = false;
                    mlog(CRITICAL, "Incomplete transfer: failed to post data record for file %s", fileName);
                    break; // early exit on error
                }
                offset += bytes_read;

                /* Calculate Checksum */
                if(parms->withChecksum)
                {
                    for(size_t i = 0; i < bytes_read; i++)
                    {
                        checksum += data->data[i];
                    }
                }
            }

            /* Send EOF Record */
            if(parms->withChecksum)
            {
                RecordObject eof_record(eofRecType);
                output_file_eof_t* eof = reinterpret_cast<output_file_eof_t*>(eof_record.getRecordData());
                StringLib::copy(&eof->filename[0], outPath, FILE_NAME_MAX_LEN);
                eof->checksum = checksum;
                if(!eof_record.post(outQ))
                {
                    status = false;
                    mlog(CRITICAL, "Failed to post eof record for file %s", fileName);
                }
            }
        } while(false);

        /* Close File */
        const int rc = fclose(fp);
        if(rc != 0)
        {
            status = false;
            char err_buf[256];
            mlog(CRITICAL, "Failed (%d) to close file %s: %s", rc, fileName, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        }
    }
    else // unable to open file
    {
        status = false;
        char err_buf[256];
        mlog(CRITICAL, "Failed (%d) to read file %s: %s", errno, fileName, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * getUniqueFileName
 *----------------------------------------------------------------------------*/
const char* OutputLib::getUniqueFileName(const char* id)
{
    char uuid_str[UUID_STR_LEN];
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);

    std::string tmp_file(TMP_FILE_PREFIX);

    if(id) tmp_file.append(id).append(".");
    else tmp_file.append("arrow.");

    tmp_file.append(uuid_str).append(".bin");
    return StringLib::duplicate(tmp_file.c_str());
}

/*----------------------------------------------------------------------------
* createMetadataFileName
*----------------------------------------------------------------------------*/
char* OutputLib::createMetadataFileName(const char* fileName)
{
    std::string path(fileName);
    const size_t dotIndex = path.find_last_of('.');
    if(dotIndex != std::string::npos)
    {
        path.resize(dotIndex);
    }
    path.append("_metadata.json");
    return StringLib::duplicate(path.c_str());
}

/*----------------------------------------------------------------------------
 * removeFile
 *----------------------------------------------------------------------------*/
void OutputLib::removeFile(const char* fileName)
{
    if(std::filesystem::exists(fileName))
    {
        const int rc = std::remove(fileName);
        if(rc != 0)
        {
            char err_buf[256];
            mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, fileName, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        }
    }
}

/*----------------------------------------------------------------------------
 * renameFile
 *----------------------------------------------------------------------------*/
void OutputLib::renameFile (const char* oldName, const char* newName)
{
    if(std::filesystem::exists(oldName))
    {
        const int rc = std::rename(oldName, newName);
        if(rc != 0)
        {
            char err_buf[256];
            mlog(CRITICAL, "Failed (%d) to rename file %s to %s: %s", rc, oldName, newName, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        }
    }
}

/*----------------------------------------------------------------------------
 * fileExists
 *----------------------------------------------------------------------------*/
bool OutputLib::fileExists(const char* fileName)
{
    return std::filesystem::exists(fileName);
}

/*----------------------------------------------------------------------------
 * isArrow -
 *----------------------------------------------------------------------------*/
bool OutputLib::isArrow (OutputFields::format_t fmt)
{
    bool status = false;
    switch(fmt)
    {
        case OutputFields::FEATHER:     status = true; break;
        case OutputFields::PARQUET:     status = true; break;
        case OutputFields::GEOPARQUET:  status = true; break;
        case OutputFields::CSV:         status = true; break;
        default:                        status = false; break;
    }
    return status;
}

/*----------------------------------------------------------------------------
 * luaSend2User -
 *----------------------------------------------------------------------------*/
int OutputLib::luaSend2User (lua_State* L)
{
    bool status = false;
    RequestFields* _parms = NULL;
    Publisher* outq = NULL;
    const char* outputpath = NULL;

    try
    {
        /* Get Parameters */
        const char* filename = LuaObject::getLuaString(L, 1);
        _parms = dynamic_cast<RequestFields*>(LuaObject::getLuaObject(L, 2, RequestFields::OBJECT_TYPE));
        const char* outq_name = LuaObject::getLuaString(L, 3);

        /* Get Trace from Lua Engine */
        lua_getglobal(L, LuaEngine::LUA_TRACEID);
        const uint32_t trace_id = lua_tonumber(L, -1);

        /* Create Publisher */
        outq = new Publisher(outq_name);

        /* Call Utility to Send File */
        status = send2User(filename, _parms->output.path.value.c_str(), trace_id, &_parms->output, outq);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sending file to user: %s", e.what());
    }

    /* Release Allocated Resources */
    if(_parms) _parms->releaseLuaObject();
    delete outq;
    delete [] outputpath;

    /* Return Status */
    lua_pushboolean(L, status);
    return 1;
}
