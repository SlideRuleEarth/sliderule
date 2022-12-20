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
#include "RasterSampler.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RasterSampler::LuaMetaName = "RasterSampler";
const struct luaL_Reg RasterSampler::LuaMetaTable[] = {
    {NULL,          NULL}
};

const char* RasterSampler::sampleRecType = "rsrec.sample";
const RecordObject::fieldDef_t RasterSampler::sampleRecDef[] = {
    {"value",           RecordObject::DOUBLE,   offsetof(VrtRaster::sample_t, value),   1,  NULL, NATIVE_FLAGS},
    {"time",            RecordObject::DOUBLE,   offsetof(VrtRaster::sample_t, time),    1,  NULL, NATIVE_FLAGS}
};

const char* RasterSampler::extentRecType = "rsrec";
const RecordObject::fieldDef_t RasterSampler::extentRecDef[] = {
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),          1,  NULL, NATIVE_FLAGS},
    {"raster_index",    RecordObject::UINT16,   offsetof(extent_t, raster_index),       1,  NULL, NATIVE_FLAGS},
    {"num_samples",     RecordObject::UINT32,   offsetof(extent_t, num_samples),        1,  NULL, NATIVE_FLAGS},
    {"samples",         RecordObject::USER,     offsetof(extent_t, samples),            0,  sampleRecType, NATIVE_FLAGS} // variable length
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :sampler(<vrt_raster>, <vrt_raster_index>, <outq name>, <rec_type>, <extent_key>, <lon_key>, <lat_key>)
 *----------------------------------------------------------------------------*/
int RasterSampler::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        VrtRaster*  _raster         = (VrtRaster*)getLuaObject(L, 1, VrtRaster::OBJECT_TYPE);
        int         raster_index    = getLuaInteger(L, 2);
        const char* outq_name       = getLuaString(L, 3);
        const char* rec_type        = getLuaString(L, 4);
        const char* extent_key      = getLuaString(L, 5);
        const char* lon_key         = getLuaString(L, 6);
        const char* lat_key         = getLuaString(L, 7);

        /* Create Dispatch */
        return createLuaObject(L, new RasterSampler(L, _raster, raster_index, outq_name, rec_type, extent_key, lon_key, lat_key));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void RasterSampler::init (void)
{
    RECDEF(sampleRecType, sampleRecDef, sizeof(VrtRaster::sample_t), NULL);
    RECDEF(extentRecType, extentRecDef, sizeof(extent_t), NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void RasterSampler::deinit (void)
{
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterSampler::RasterSampler (lua_State* L, VrtRaster* _raster, int raster_index, const char* outq_name, const char* rec_type, const char* extent_key, const char* lon_key, const char* lat_key):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(_raster);
    assert(outq_name);
    assert(lon_key);
    assert(lat_key);

    raster = _raster;
    rasterIndex = raster_index;

    outQ = new Publisher(outq_name);

    extentSizeBytes = RecordObject::getRecordDataSize(rec_type);
    if(extentSizeBytes <= 0)
    {
        mlog(CRITICAL, "Failed to get size of extent for record type: %s", rec_type);
    }

    extentField = RecordObject::getDefinedField(rec_type, extent_key);
    if(extentField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", extent_key, rec_type);
    }

    lonField = RecordObject::getDefinedField(rec_type, lon_key);
    if(lonField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", lon_key, rec_type);
    }

    latField = RecordObject::getDefinedField(rec_type, lat_key);
    if(latField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", lat_key, rec_type);
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
RasterSampler::~RasterSampler(void)
{
    raster->releaseLuaObject();
    delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *
 *  OUTPUT: one extent_t record per extent_id
 *  INPUT:  batch of atl06 extents
 *          each extent (up to 256 per record) will produce a single output record with one point
 *          that one point may have multiple samples associated with it
 *----------------------------------------------------------------------------*/
bool RasterSampler::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    bool status = true;

    /* Determine Number of Rows in Record */
    int record_size_bytes = record->getAllocatedDataSize();
    int num_extents = record_size_bytes / extentSizeBytes;
    int left_over = record_size_bytes % extentSizeBytes;
    if(left_over > 0)
    {
        mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", record->getRecordType(), record_size_bytes, extentSizeBytes);
        return false;
    }

    /* Initialize Local Fields */
    RecordObject::field_t extent_field = extentField;
    RecordObject::field_t lon_field = lonField;
    RecordObject::field_t lat_field = latField;

    /* Loop Through Each Record in Batch */
    for(int extent = 0; extent < num_extents; extent++)
    {
        /* Get Extent Id */
        uint64_t extent_id = (uint64_t)record->getValueInteger(extent_field);
        extent_field.offset += (extentSizeBytes * 8);

        /* Get Longitude */
        double lon_val = record->getValueReal(lon_field);
        lon_field.offset += (extentSizeBytes * 8);

        /* Get Latitude */
        double lat_val = record->getValueReal(lat_field);
        lat_field.offset += (extentSizeBytes * 8);

        /* Sample Raster */
        List<VrtRaster::sample_t> slist;
        int num_samples = raster->sample(lon_val, lat_val, slist);

        /* Create Sample Record */
        int size_of_record = offsetof(extent_t, samples) + (sizeof(VrtRaster::sample_t) * num_samples);
        RecordObject sample_rec(extentRecType, size_of_record);
        extent_t* data = (extent_t*)sample_rec.getRecordData();
        data->extent_id = extent_id;
        data->raster_index = rasterIndex;
        data->num_samples = num_samples;
        for(int i = 0; i < num_samples; i++)
        {
            data->samples[i] = slist[i];
        }

        /* Post Sample Record */
        uint8_t* rec_buf = NULL;
        int rec_bytes = sample_rec.serialize(&rec_buf, RecordObject::TAKE_OWNERSHIP);
        int post_status = MsgQ::STATE_TIMEOUT;
        while((post_status = outQ->postRef(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
        if(post_status <= 0)
        {
            delete [] rec_buf; // we've taken ownership
            mlog(ERROR, "Raster sampler failed to post %s to stream %s: %d", sample_rec.getRecordType(), outQ->getName(), post_status);
            status = false;
        }
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool RasterSampler::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool RasterSampler::processTermination (void)
{
    return true;
}
