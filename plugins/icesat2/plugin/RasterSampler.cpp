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

const char* RasterSampler::sampleRecType = "samplerec";
const RecordObject::fieldDef_t RasterSampler::sampleRecDef[] = {
    {"extent_id",   RecordObject::INT64,    offsetof(sample_extent_t, extent_id),                                       1,  NULL, NATIVE_FLAGS},
    {"value",       RecordObject::DOUBLE,   offsetof(sample_extent_t, sample) + offsetof(VrtRaster::sample_t, value),   1,  NULL, NATIVE_FLAGS},
    {"time",        RecordObject::DOUBLE,   offsetof(sample_extent_t, sample) + offsetof(VrtRaster::sample_t, time),    1,  NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :sampler(<vrt_raster>, <outq name>, <lon_key>, <lat_key>)
 *----------------------------------------------------------------------------*/
int RasterSampler::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        VrtRaster*  _raster     = (VrtRaster*)getLuaObject(L, 1, VrtRaster::OBJECT_TYPE);
        const char* outq_name   = getLuaString(L, 2);
        const char* lon_key     = getLuaString(L, 3);
        const char* lat_key     = getLuaString(L, 4);

        /* Create Dispatch */
        return createLuaObject(L, new RasterSampler(L, _raster, outq_name, lon_key, lat_key));
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
    RECDEF(sampleRecType, sampleRecDef, sizeof(sample_extent_t), NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void RasterSampler::deinit (void)
{
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterSampler::RasterSampler (lua_State* L, VrtRaster* _raster, const char* outq_name, const char* lon_key, const char* lat_key):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(_raster);
    assert(outq_name);
    assert(lon_key);
    assert(lat_key);

    raster = _raster;
    outQ = new Publisher(outq_name);
    lonKey = StringLib::duplicate(lon_key);
    latKey = StringLib::duplicate(lat_key);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
RasterSampler::~RasterSampler(void)
{
    raster->releaseLuaObject();
    delete outQ;
    delete [] lonKey;
    delete [] latKey;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool RasterSampler::processRecord (RecordObject* record, okey_t key)
{
    bool status = true;

    /* Get Longitude */
    RecordObject::field_t lon_field = record->getField(lonKey);
    double lon_val = record->getValueReal(lon_field);

    /* Get Latitude */
    RecordObject::field_t lat_field = record->getField(latKey);
    double lat_val = record->getValueReal(lat_field);

    /* Send Samples */
    List<VrtRaster::sample_t> slist;
    int num_samples = raster->sample(lon_val, lat_val, slist);
    for(int i = 0; i < num_samples; i++)
    {
        /* Create Sample Record */
        RecordObject sample_rec(sampleRecType);
        sample_extent_t* data = (sample_extent_t*)sample_rec.getRecordData();

        /* Populate Sample */
        data->extent_id = (int64_t)key;
        data->sample = slist[i];

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
