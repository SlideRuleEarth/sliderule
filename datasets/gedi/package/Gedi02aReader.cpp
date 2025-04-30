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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "OsApi.h"
#include "H5Coro.h"
#include "Gedi02aReader.h"
#include "RecordObject.h"
#include "FootprintReader.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi02aReader::fpRecType = "gedi02arec.footprint";
const RecordObject::fieldDef_t Gedi02aReader::fpRecDef[] = {
    {"shot_number",     RecordObject::UINT64,   offsetof(g02a_footprint_t, shot_number),             1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"time",            RecordObject::TIME8,    offsetof(g02a_footprint_t, time_ns),                 1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"latitude",        RecordObject::DOUBLE,   offsetof(g02a_footprint_t, latitude),                1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(g02a_footprint_t, longitude),               1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"elevation_lm",    RecordObject::FLOAT,    offsetof(g02a_footprint_t, elevation_lowestmode),    1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"elevation_hr",    RecordObject::FLOAT,    offsetof(g02a_footprint_t, elevation_highestreturn), 1,  NULL, NATIVE_FLAGS},
    {"solar_elevation", RecordObject::FLOAT,    offsetof(g02a_footprint_t, solar_elevation),         1,  NULL, NATIVE_FLAGS},
    {"sensitivity",     RecordObject::FLOAT,    offsetof(g02a_footprint_t, sensitivity),             1,  NULL, NATIVE_FLAGS},
    {"beam",            RecordObject::UINT8,    offsetof(g02a_footprint_t, beam),                    1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT8,    offsetof(g02a_footprint_t, flags),                   1,  NULL, NATIVE_FLAGS},
    {"track",           RecordObject::UINT16,   offsetof(g02a_footprint_t, track),                   1,  NULL, NATIVE_FLAGS}
};

const char* Gedi02aReader::batchRecType = "gedi02arec";
const RecordObject::fieldDef_t Gedi02aReader::batchRecDef[] = {
    {"footprint",       RecordObject::USER,     offsetof(batch_t, footprint),         0,  fpRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

/******************************************************************************
 * GEDI L2A READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Gedi02aReader::luaCreate (lua_State* L)
{
    GediFields* parms = NULL;

    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        parms = dynamic_cast<GediFields*>(getLuaObject(L, 2, GediFields::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 3, true, true);

        /* Check for Null Resource and Asset */
        if(parms->resource.value.empty()) throw RunTimeException(CRITICAL, RTE_FAILURE, "Must supply a resource to process");
        else if(parms->asset.asset == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Must supply a valid asset");

        /* Return Reader Object */
        return createLuaObject(L, new Gedi02aReader(L, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating Gedi02aReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Gedi02aReader::init (void)
{
    RECDEF(fpRecType,       fpRecDef,       sizeof(g02a_footprint_t),           NULL);
    RECDEF(batchRecType,    batchRecDef,    offsetof(batch_t, footprint[1]),    NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::Gedi02aReader (lua_State* L, const char* outq_name, GediFields* _parms, bool _send_terminator):
    FootprintReader<g02a_footprint_t>(L, outq_name, _parms, _send_terminator,
                                      batchRecType, "lat_lowestmode", "lon_lowestmode",
                                      Gedi02aReader::subsettingThread)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::~Gedi02aReader (void) = default;

/*----------------------------------------------------------------------------
 * Gedi02a::Constructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::Gedi02a::Gedi02a (const info_t* info, const Region& region):
    shot_number         (info->reader->context, FString("%s/shot_number",       info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    delta_time          (info->reader->context, FString("%s/delta_time",        info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    elev_lowestmode     (info->reader->context, FString("%s/elev_lowestmode",   info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    elev_highestreturn  (info->reader->context, FString("%s/elev_highestreturn",info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    solar_elevation     (info->reader->context, FString("%s/solar_elevation",   info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    sensitivity         (info->reader->context, FString("%s/sensitivity",       info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    degrade_flag        (info->reader->context, FString("%s/degrade_flag",      info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    quality_flag        (info->reader->context, FString("%s/quality_flag",      info->group).c_str(), 0, region.first_footprint, region.num_footprints),
    surface_flag        (info->reader->context, FString("%s/surface_flag",      info->group).c_str(), 0, region.first_footprint, region.num_footprints)
{

    /* Join Hardcoded Reads */
    shot_number.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    elev_lowestmode.join(info->reader->read_timeout_ms, true);
    elev_highestreturn.join(info->reader->read_timeout_ms, true);
    solar_elevation.join(info->reader->read_timeout_ms, true);
    sensitivity.join(info->reader->read_timeout_ms, true);
    degrade_flag.join(info->reader->read_timeout_ms, true);
    quality_flag.join(info->reader->read_timeout_ms, true);
    surface_flag.join(info->reader->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Gedi02a::Destructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::Gedi02a::~Gedi02a (void) = default;

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi02aReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Gedi02aReader* reader = reinterpret_cast<Gedi02aReader*>(info->reader); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    const GediFields* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "Gedi02a_reader", "{\"asset\":\"%s\", \"resource\":\"%s\", \"beam\":%d}", parms->asset.getName(), parms->getResource(), info->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read GEDI Datasets */
        const Gedi02a gedi02a(info, region);
        if(!reader->readAncillaryData(info, region.first_footprint, region.num_footprints))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to read ancillary data");
        }

        /* Increment Read Statistics */
        local_stats.footprints_read = region.num_footprints;

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; reader->active && footprint < region.num_footprints; footprint++)
        {
            /* Check Degrade Filter */
            if(parms->degrade_filter)
            {
                if(gedi02a.degrade_flag[footprint])
                {
                    local_stats.footprints_filtered++;
                    continue;
                }
            }

            /* Check L2 Quality Filter */
            if(parms->l2_quality_filter)
            {
                if(!gedi02a.quality_flag[footprint])
                {
                    local_stats.footprints_filtered++;
                    continue;
                }
            }

            /* Check Surface Filter */
            if(parms->surface_filter)
            {
                if(!gedi02a.surface_flag[footprint])
                {
                    local_stats.footprints_filtered++;
                    continue;
                }
            }

            /* Check Region */
            if(region.inclusion_ptr)
            {
                if(!region.inclusion_ptr[footprint])
                {
                    continue;
                }
            }

            reader->threadMut.lock();
            {
                /* Populate Entry in Batch Structure */
                g02a_footprint_t* fp = &reader->batchData->footprint[reader->batchIndex];
                fp->shot_number             = gedi02a.shot_number[footprint];
                fp->time_ns                 = GediFields::deltatime2timestamp(gedi02a.delta_time[footprint]);
                fp->latitude                = region.lat[footprint];
                fp->longitude               = region.lon[footprint];
                fp->elevation_lowestmode    = gedi02a.elev_lowestmode[footprint];
                fp->elevation_highestreturn = gedi02a.elev_highestreturn[footprint];
                fp->solar_elevation         = gedi02a.solar_elevation[footprint];
                fp->sensitivity             = gedi02a.sensitivity[footprint];
                fp->beam                    = static_cast<uint8_t>(info->beam);
                fp->flags                   = 0;
                fp->track                   = parms->granule_fields.track.value;
                if(gedi02a.degrade_flag[footprint]) fp->flags |= GediFields::DEGRADE_FLAG_MASK;
                if(gedi02a.quality_flag[footprint]) fp->flags |= GediFields::L2_QUALITY_FLAG_MASK;
                if(gedi02a.surface_flag[footprint]) fp->flags |= GediFields::SURFACE_FLAG_MASK;

                /* Populate Ancillary Fields */
                reader->populateAncillaryFields(info, footprint, fp->shot_number);

                /* Send Record */
                reader->batchIndex++;
                if(reader->batchIndex >= BATCH_SIZE)
                {
                    reader->postRecordBatch(&local_stats);
                    reader->batchIndex = 0;
                }
            }
            reader->threadMut.unlock();
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s beam %d: %s", parms->getResource(), static_cast<int>(info->beam), e.what());
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Count Completion */
        reader->numComplete++;

        /* Send Final Record Batch */
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", parms->getResource());
            if(reader->batchIndex > 0)
            {
                reader->postRecordBatch(&local_stats);
            }
        }

        /* Update Statistics */
        reader->stats.footprints_read += local_stats.footprints_read;
        reader->stats.footprints_filtered += local_stats.footprints_filtered;
        reader->stats.footprints_sent += local_stats.footprints_sent;
        reader->stats.footprints_dropped += local_stats.footprints_dropped;
        reader->stats.footprints_retried += local_stats.footprints_retried;

        /* Indicate End of Data */
        if(reader->numComplete == reader->threadCount)
        {
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, parms->getResource());
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", parms->getResource());
                    }
                }
            }
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up Info */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
