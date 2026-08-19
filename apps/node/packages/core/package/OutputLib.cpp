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
#include "RequestParameters.h"
#include "OutputLib.h"
#include "RecordObject.h"
#include "CredentialStore.h"

#ifdef __aws__
#include "S3CurlIODriver.h"
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
    {"filename",   RecordObject::STRING,   offsetof(output_file_meta_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS, "output file name"},
    {"size",       RecordObject::INT64,    offsetof(output_file_meta_t, size),                      1,  NULL, NATIVE_FLAGS, "size in bytes of output file"}
};

const RecordObject::fieldDef_t OutputLib::dataRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(output_file_data_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS, "output file name"},
    {"data",       RecordObject::UINT8,    offsetof(output_file_data_t, data),                      0,  NULL, NATIVE_FLAGS, "transmitted data contents of file"} // variable length
};

const RecordObject::fieldDef_t OutputLib::eofRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(output_file_eof_t, filename),   FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS, "output file name"},
    {"checksum",   RecordObject::UINT64,   offsetof(output_file_eof_t, checksum),                   1,  NULL, NATIVE_FLAGS, "checksum of contents of file (64-bit byte-wise sum)"}
};

const RecordObject::fieldDef_t OutputLib::remoteRecDef[] = {
    {"url",   RecordObject::STRING,   offsetof(output_file_remote_t, url),                URL_MAX_LEN,  NULL, NATIVE_FLAGS, "URL of output file when file is remote (e.g. s3://sliderule-public/my-data-run.parquet)"},
    {"size",  RecordObject::INT64,    offsetof(output_file_remote_t, size),                         1,  NULL, NATIVE_FLAGS, "size in bytes of remote output file"}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void OutputLib::init (void)
{
    RECDEF(metaRecType, metaRecDef, sizeof(output_file_meta_t), NULL);
    RECDEF(dataRecType, dataRecDef, sizeof(output_file_data_t), NULL);
    RECDEF(eofRecType, eofRecDef, sizeof(output_file_eof_t), NULL);
    RECDEF(remoteRecType, remoteRecDef, sizeof(output_file_remote_t), NULL);
}

/*----------------------------------------------------------------------------
 * send2User
 *----------------------------------------------------------------------------*/
bool OutputLib::send2User (const char* src_file, const string& output_path, uint32_t trace_id, const OutputFields& output_fields, const char* asset_name, bool with_checksum, Publisher* outq)
{
    bool status = false;
    const uint32_t send_trace_id = start_trace(INFO, trace_id, "send_file", "{\"path\": \"%s\"}", dst_file);

    /* Send File to User */
    if(asset_name)
    {
        /* Upload File to S3 Asset */
        Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));
        if(!asset)
        {
            mlog(CRITICAL, "Unable to output file <%s>, failed to retrieve asset <%s>", src_file, asset_name);
            status = false;
        }
        else if(!StringLib::match(asset->getDriver(), "s3"))
        {
            mlog(CRITICAL, "Unable to output file <%s>, unsupported driver <%s>", src_file, asset->getDriver());
            asset->releaseLuaObject();
            status = false;
        }
        else
        {
            const char* endpoint = asset->getEndpoint();
            const CredentialStore::Credential& credentials = CredentialStore::get(asset->getIdentity());
            status = send2S3(src_file, FString("%s/%s", asset->getPath(), output_path.c_str()).c_str(), endpoint, credentials, with_checksum, outq);
            asset->releaseLuaObject();
        }
    }
    else if(output_path.starts_with("s3://"))
    {
        /* Upload File to User Supplied S3 Bucket */
        status = send2S3(src_file, output_path.substr(5).c_str(), output_fields.endpoint.value.c_str(), output_fields.credentials, with_checksum, outq);
    }
    else if(output_path.starts_with("file://"))
    {
        /* Rename File (local) */
        status = renameFile(src_file, output_path.substr(7).c_str());
    }
    else
    {
        /* Stream File Back to Client */
        status = send2Client(src_file, output_path.c_str(), with_checksum, outq);
    }

    /* Delete File Locally */
    removeFile(src_file);

    stop_trace(INFO, send_trace_id);
    return status;
}

/*----------------------------------------------------------------------------
 * send2S3
 *----------------------------------------------------------------------------*/
bool OutputLib::send2S3 (const char* src_file, const char* dst_file, const char* endpoint, const CredentialStore::Credential& credentials, bool with_checksum, Publisher* outq)
{
    #ifdef __aws__

    bool status = true;

    /* Check Path */
    if(!dst_file) return false;

    /* Get Bucket and Key */
    char* bucket = StringLib::duplicate(dst_file);
    char* key = bucket;
    while(*key != '\0' && *key != '/') key++;
    if(*key == '/')
    {
        *key = '\0';
    }
    else
    {
        status = false;
        mlog(CRITICAL, "invalid S3 url: %s", dst_file);
    }
    key++;

    /* Put File */
    if(status)
    {
        /* Send Initial Status */
        alert(INFO, RTE_STATUS, outq, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        /* Upload to S3 */
        int attempt = 0;
        int64_t bytes_uploaded = 0;
        while(bytes_uploaded == 0 && attempt++ < S3CurlIODriver::ATTEMPTS_PER_REQUEST)
        {
            try
            {
                bytes_uploaded = S3CurlIODriver::put(src_file, bucket, key, endpoint, &credentials, with_checksum);
            }
            catch(const RunTimeException& e)
            {
                alert(e.level(), RTE_FAILURE, outq, NULL, "S3 PUT failed attempt %d, bucket = %s, key = %s, error = %s", attempt, bucket, key, e.what());
            }
        }

        if(bytes_uploaded > 0)
        {
            /* Send Successful Status */
            alert(INFO, RTE_STATUS, outq, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);

            /* Send Remote Record */
            RecordObject remote_record(remoteRecType);
            output_file_remote_t* remote = reinterpret_cast<output_file_remote_t*>(remote_record.getRecordData());
            StringLib::copy(&remote->url[0], FString("s3://%s", dst_file).c_str(), URL_MAX_LEN);
            remote->size = bytes_uploaded;
            if(!remote_record.post(outq))
            {
                mlog(CRITICAL, "Failed to send remote record back to user for %s", dst_file);
            }
        }
        else
        {
            /* Set Error Status */
            status = false;

            /* Send Error Status */
            alert(CRITICAL, RTE_FAILURE, outq, NULL, "Upload to S3 failed, bucket = %s, key = %s", bucket, key);
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    alert(CRITICAL, RTE_FAILURE, outq, NULL, "Output path specifies S3, but server compiled without AWS support");
    return false;
    #endif
}

/*----------------------------------------------------------------------------
 * send2Client
 *----------------------------------------------------------------------------*/
bool OutputLib::send2Client (const char* src_file, const char* dst_file, bool with_checksum, Publisher* outq)
{
    bool status = true;

    /* Reopen File to Stream Back as Response */
    FILE* fp = fopen(src_file, "r");
    if(fp)
    {
        /* Get Size of File */
        fseek(fp, 0L, SEEK_END);
        const long file_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        /* Log Status */
        mlog(INFO, "Sending file %s of size %ld to %s", src_file, file_size, dst_file);

        do
        {
            uint64_t checksum = 0;

            /* Send Meta Record */
            RecordObject meta_record(metaRecType);
            output_file_meta_t* meta = reinterpret_cast<output_file_meta_t*>(meta_record.getRecordData());
            StringLib::copy(&meta->filename[0], dst_file, FILE_NAME_MAX_LEN);
            meta->size = file_size;
            if(!meta_record.post(outq))
            {
                status = false;
                mlog(CRITICAL, "Failed to post meta record for file %s", src_file);
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
                StringLib::copy(&data->filename[0], dst_file, FILE_NAME_MAX_LEN);
                const size_t bytes_read = fread(data->data, 1, bytes_to_send, fp);
                if(!data_record.post(outq, offsetof(output_file_data_t, data) + bytes_read))
                {
                    status = false;
                    mlog(CRITICAL, "Incomplete transfer: failed to post data record for file %s", src_file);
                    break; // early exit on error
                }
                offset += bytes_read;

                /* Calculate Checksum */
                if(with_checksum)
                {
                    for(size_t i = 0; i < bytes_read; i++)
                    {
                        checksum += data->data[i];
                    }
                }
            }

            /* Send EOF Record */
            if(with_checksum)
            {
                RecordObject eof_record(eofRecType);
                output_file_eof_t* eof = reinterpret_cast<output_file_eof_t*>(eof_record.getRecordData());
                StringLib::copy(&eof->filename[0], dst_file, FILE_NAME_MAX_LEN);
                eof->checksum = checksum;
                if(!eof_record.post(outq))
                {
                    status = false;
                    mlog(CRITICAL, "Failed to post eof record for file %s", src_file);
                }
            }
        } while(false);

        /* Close File */
        const int rc = fclose(fp);
        if(rc != 0)
        {
            status = false;
            char err_buf[256];
            mlog(CRITICAL, "Failed (%d) to close file %s: %s", rc, src_file, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        }
    }
    else // unable to open file
    {
        status = false;
        char err_buf[256];
        mlog(CRITICAL, "Failed (%d) to read file %s: %s", errno, src_file, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * getUniqueFileName
 *----------------------------------------------------------------------------*/
const char* OutputLib::getUniqueFileName (const char* id)
{
    string tmp_file(TMP_FILE_PREFIX);

    if(id) tmp_file.append(id).append(".");
    else tmp_file.append("arrow.");

    tmp_file.append(UString().c_str()).append(".bin");
    return StringLib::duplicate(tmp_file.c_str());
}

/*----------------------------------------------------------------------------
 * removeFile
 *----------------------------------------------------------------------------*/
void OutputLib::removeFile (const char* src_file)
{
    if(std::filesystem::exists(src_file))
    {
        const int rc = std::remove(src_file);
        if(rc != 0)
        {
            char err_buf[256];
            mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, src_file, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        }
    }
}

/*----------------------------------------------------------------------------
 * renameFile
 *----------------------------------------------------------------------------*/
bool OutputLib::renameFile (const char* old_name, const char* new_name)
{
    if(!std::filesystem::exists(old_name))
    {
        mlog(CRITICAL, "Failed to rename file %s to %s: source does not exist", old_name, new_name);
        return false;
    }

    const int rc = std::rename(old_name, new_name);
    if(rc != 0)
    {
        char err_buf[256];
        mlog(CRITICAL, "Failed (%d) to rename file %s to %s: %s", rc, old_name, new_name, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * fileExists
 *----------------------------------------------------------------------------*/
bool OutputLib::fileExists (const char* src_file)
{
    return std::filesystem::exists(src_file);
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
 * isLas -
 *----------------------------------------------------------------------------*/
bool OutputLib::isLas (OutputFields::format_t fmt)
{
    return (fmt == OutputFields::LAS) || (fmt == OutputFields::LAZ);
}

/*----------------------------------------------------------------------------
 * luaSend2User -
 *----------------------------------------------------------------------------*/
int OutputLib::luaSend2User (lua_State* L)
{
    bool status = false;
    RequestParameters* _parms = NULL;
    Publisher* outq = NULL;

    try
    {
        /* Get Parameters */
        const char* source_filename = LuaObject::getLuaString(L, 1);
        const char* outq_name = LuaObject::getLuaString(L, 2);
        _parms = dynamic_cast<RequestParameters*>(LuaObject::getLuaObject(L, 3, RequestParameters::OBJECT_TYPE));
        const char* destination_filename = LuaObject::getLuaString(L, 4, true, _parms->output.path.value.c_str()); // override
        const char* asset_name = LuaObject::getLuaString(L, 5, true, _parms->output.assetName.value.c_str()); // override
        const bool with_checksum = LuaObject::getLuaBoolean(L, 6, true, _parms->output.withChecksum.value); // override
        const char* with_suffix = LuaObject::getLuaString(L, 7, true, ".bin");

        /* Get Trace from Lua Engine */
        lua_getglobal(L, LuaEngine::LUA_TRACEID);
        const uint32_t trace_id = lua_tonumber(L, -1);

        /* Create Publisher */
        outq = new Publisher(outq_name);

        /* (Optionally) Generate Output Path */
        string output_path = destination_filename;
        if((destination_filename == NULL) || (destination_filename[0] == '\0'))
        {
            output_path = FString("%s.%016lX%s", SystemConfig::settings().cluster.value.c_str(), OsApi::time(OsApi::CPU_CLK), with_suffix).c_str();
            mlog(DEBUG, "Generating unique path: %s", output_path.c_str());
        }

        /* Call Utility to Send File */
        status = send2User(source_filename, output_path, trace_id, _parms->output, asset_name, with_checksum, outq);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sending file to user: %s", e.what());
    }

    /* Release Allocated Resources */
    if(_parms) _parms->releaseLuaObject();
    delete outq;

    /* Return Status */
    lua_pushboolean(L, status);
    return 1;
}
