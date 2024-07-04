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
#include "ArrowCommon.h"

#include <filesystem>
#include <uuid/uuid.h>

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * TYPES
 ******************************************************************************/

static const int URL_MAX_LEN = 512;
static const int FILE_NAME_MAX_LEN = 128;
static const int FILE_BUFFER_RSPS_SIZE = 0x2000000; // 32MB

typedef struct {
    char        filename[FILE_NAME_MAX_LEN];
    long        size;
} arrow_file_meta_t;

typedef struct {
    char        filename[FILE_NAME_MAX_LEN];
    uint8_t     data[FILE_BUFFER_RSPS_SIZE];
} arrow_file_data_t;

typedef struct {
    char        filename[FILE_NAME_MAX_LEN];
    uint64_t    checksum;
} arrow_file_eof_t;

typedef struct {
    char        url[URL_MAX_LEN];
    long        size;
} arrow_file_remote_t;


/******************************************************************************
 * CONSTANTS
 ******************************************************************************/

static const char* metaRecType   = "arrowrec.meta";
static const char* dataRecType   = "arrowrec.data";
static const char* eofRecType    = "arrowrec.eof";
static const char* remoteRecType = "arrowrec.remote";

static const RecordObject::fieldDef_t metaRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_meta_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",       RecordObject::INT64,    offsetof(arrow_file_meta_t, size),                      1,  NULL, NATIVE_FLAGS}
};

static const RecordObject::fieldDef_t dataRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_data_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"data",       RecordObject::UINT8,    offsetof(arrow_file_data_t, data),                      0,  NULL, NATIVE_FLAGS} // variable length
};

static const RecordObject::fieldDef_t eofRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_eof_t, filename),   FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"checksum",   RecordObject::UINT64,   offsetof(arrow_file_eof_t, checksum),                   1,  NULL, NATIVE_FLAGS}
};

static const RecordObject::fieldDef_t remoteRecDef[] = {
    {"url",   RecordObject::STRING,   offsetof(arrow_file_remote_t, url),                URL_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",  RecordObject::INT64,    offsetof(arrow_file_remote_t, size),                         1,  NULL, NATIVE_FLAGS}
};

static const char* TMP_FILE_PREFIX = "/tmp/";



/******************************************************************************
 * NAMESPACES
 ******************************************************************************/
namespace ArrowCommon
{

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void init(void)
{
    static bool initialized = false;
    if(initialized) return;

    initialized = true;
    RECDEF(metaRecType, metaRecDef, sizeof(arrow_file_meta_t), NULL);
    RECDEF(dataRecType, dataRecDef, sizeof(arrow_file_data_t), NULL);
    RECDEF(eofRecType, eofRecDef, sizeof(arrow_file_eof_t), NULL);
    RECDEF(remoteRecType, remoteRecDef, sizeof(arrow_file_remote_t), NULL);
}

/*----------------------------------------------------------------------------
 * send2user
 *----------------------------------------------------------------------------*/
bool send2User (const char* fileName, const char* outputPath,
                uint32_t traceId, ArrowParms* parms, Publisher* outQ)
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
bool send2S3 (const char* fileName, const char* s3dst, const char* outputPath,
              ArrowParms* parms, Publisher* outQ)
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
        alert(INFO, RTE_INFO, outQ, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        try
        {
            /* Upload to S3 */
            const int64_t bytes_uploaded = S3CurlIODriver::put(fileName, bucket, key, parms->region, &parms->credentials);

            /* Send Successful Status */
            alert(INFO, RTE_INFO, outQ, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);

            /* Send Remote Record */
            RecordObject remote_record(remoteRecType);
            arrow_file_remote_t* remote = reinterpret_cast<arrow_file_remote_t*>(remote_record.getRecordData());
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
            alert(e.level(), RTE_ERROR, outQ, NULL, "Upload to S3 failed, bucket = %s, key = %s, error = %s", bucket, key, e.what());
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    alert(CRITICAL, RTE_ERROR, outQ, NULL, "Output path specifies S3, but server compiled without AWS support");
    return false;
    #endif
}

/*----------------------------------------------------------------------------
 * send2Client
 *----------------------------------------------------------------------------*/
bool send2Client (const char* fileName, const char* outPath, const ArrowParms* parms, Publisher* outQ)
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
        mlog(INFO, "Writing file %s of size %ld", fileName, file_size);

        do
        {
            uint64_t checksum = 0;

            /* Send Meta Record */
            RecordObject meta_record(metaRecType);
            arrow_file_meta_t* meta = reinterpret_cast<arrow_file_meta_t*>(meta_record.getRecordData());
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
                RecordObject data_record(dataRecType, 0, false);
                arrow_file_data_t* data = reinterpret_cast<arrow_file_data_t*>(data_record.getRecordData());
                StringLib::copy(&data->filename[0], outPath, FILE_NAME_MAX_LEN);
                const size_t bytes_read = fread(data->data, 1, FILE_BUFFER_RSPS_SIZE, fp);
                if(!data_record.post(outQ, offsetof(arrow_file_data_t, data) + bytes_read))
                {
                    status = false;
                    mlog(CRITICAL, "Incomplete transfer: failed to post data record for file %s", fileName);
                    break; // early exit on error
                }
                offset += bytes_read;

                /* Calculate Checksum */
                if(parms->with_checksum)
                {
                    for(size_t i = 0; i < bytes_read; i++)
                    {
                        checksum += data->data[i];
                    }
                }
            }

            /* Send EOF Record */
            if(parms->with_checksum)
            {
                RecordObject eof_record(eofRecType);
                arrow_file_eof_t* eof = reinterpret_cast<arrow_file_eof_t*>(eof_record.getRecordData());
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
 * getOutputPath
 *----------------------------------------------------------------------------*/
const char* getOutputPath(ArrowParms* parms)
{
    const char* outputPath = NULL;

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
        const char* path_suffix = "bin";
        if(parms->format == ArrowParms::PARQUET) path_suffix = parms->as_geo ? ".geoparquet" : ".parquet";
        else if(parms->format == ArrowParms::CSV) path_suffix = ".csv";
        FString path_name("%s.%016lX%s", OsApi::getCluster(), OsApi::time(OsApi::CPU_CLK), path_suffix);
        const bool use_provided_path = ((parms->path != NULL) && (parms->path[0] != '\0'));
        FString path_str("%s%s/%s", path_prefix, asset->getPath(), use_provided_path ? parms->path : path_name.c_str());
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

    return outputPath;
}

/*----------------------------------------------------------------------------
 * getUniqueFileName
 *----------------------------------------------------------------------------*/
const char* getUniqueFileName(const char* id)
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
char* createMetadataFileName(const char* fileName)
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
void removeFile(const char* fileName)
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
void renameFile (const char* oldName, const char* newName)
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
bool fileExists(const char* fileName)
{
    return std::filesystem::exists(fileName);
}

/*----------------------------------------------------------------------------
 * luaSend2User -
 *----------------------------------------------------------------------------*/
int luaSend2User (lua_State* L)
{
    bool status = false;
    ArrowParms* _parms = NULL;
    Publisher* outq = NULL;
    const char* outputpath = NULL;

    try
    {
        /* Get Parameters */
        const char* filename = LuaObject::getLuaString(L, 1);
        _parms = dynamic_cast<ArrowParms*>(LuaObject::getLuaObject(L, 2, ArrowParms::OBJECT_TYPE));
        const char* outq_name = LuaObject::getLuaString(L, 3);

        /* Get Output Path */
        outputpath = getOutputPath(_parms);

        /* Get Trace from Lua Engine */
        lua_getglobal(L, LuaEngine::LUA_TRACEID);
        const uint32_t trace_id = lua_tonumber(L, -1);

        /* Create Publisher */
        outq = new Publisher(outq_name);

        /* Call Utility to Send File */
        status = send2User(filename, outputpath, trace_id, _parms, outq);
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

} /* namespace ArrowCommon */
