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

#include "core.h"
#include "h5.h"
#include "gedi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi02aReader::fpRecType = "gedi02arec.footprint";
const RecordObject::fieldDef_t Gedi02aReader::fpRecDef[] = {
    {"shot_number",     RecordObject::UINT64,   offsetof(g02a_footprint_t, shot_number),             1,  NULL, NATIVE_FLAGS},
    {"time",            RecordObject::TIME8,    offsetof(g02a_footprint_t, time_ns),                 1,  NULL, NATIVE_FLAGS},
    {"latitude",        RecordObject::DOUBLE,   offsetof(g02a_footprint_t, latitude),                1,  NULL, NATIVE_FLAGS},
    {"longitude",       RecordObject::DOUBLE,   offsetof(g02a_footprint_t, longitude),               1,  NULL, NATIVE_FLAGS},
    {"elevation_lm",    RecordObject::FLOAT,    offsetof(g02a_footprint_t, elevation_lowestmode),    1,  NULL, NATIVE_FLAGS},
    {"elevation_hr",    RecordObject::FLOAT,    offsetof(g02a_footprint_t, elevation_highestreturn), 1,  NULL, NATIVE_FLAGS},
    {"solar_elevation", RecordObject::FLOAT,    offsetof(g02a_footprint_t, solar_elevation),         1,  NULL, NATIVE_FLAGS},
    {"sensitivity",     RecordObject::FLOAT,    offsetof(g02a_footprint_t, sensitivity),             1,  NULL, NATIVE_FLAGS},
    {"beam",            RecordObject::UINT8,    offsetof(g02a_footprint_t, beam),                    1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT8,    offsetof(g02a_footprint_t, flags),                   1,  NULL, NATIVE_FLAGS}
};

const char* Gedi02aReader::batchRecType = "gedi02arec";
const RecordObject::fieldDef_t Gedi02aReader::batchRecDef[] = {
    {"footprint",       RecordObject::USER,     offsetof(batch_t, footprint),         0,  fpRecType, NATIVE_FLAGS}
};

/******************************************************************************
 * GEDI L2A READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Gedi02aReader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    GediParms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = (GediParms*)getLuaObject(L, 4, GediParms::OBJECT_TYPE);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Gedi02aReader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
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
Gedi02aReader::Gedi02aReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator):
    FootprintReader<g02a_footprint_t>(L, _asset, _resource,
                                      outq_name, _parms, _send_terminator,
                                      batchRecType, "lat_lowestmode", "lon_lowestmode",
                                      Gedi02aReader::subsettingThread)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::~Gedi02aReader (void)
{
}

/*----------------------------------------------------------------------------
 * Gedi02a::Constructor
 *----------------------------------------------------------------------------*/
Gedi02aReader::Gedi02a::Gedi02a (info_t* info, Region& region):
    shot_number     (info->reader->asset, info->reader->resource, SafeString("%s/shot_number",      GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    delta_time      (info->reader->asset, info->reader->resource, SafeString("%s/delta_time",       GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    elev_lowestmode (info->reader->asset, info->reader->resource, SafeString("%s/elev_lowestmode",  GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    elev_highestreturn (info->reader->asset, info->reader->resource, SafeString("%s/elev_highestreturn",  GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    solar_elevation (info->reader->asset, info->reader->resource, SafeString("%s/solar_elevation",  GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    sensitivity     (info->reader->asset, info->reader->resource, SafeString("%s/sensitivity",      GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    degrade_flag    (info->reader->asset, info->reader->resource, SafeString("%s/degrade_flag",     GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    quality_flag    (info->reader->asset, info->reader->resource, SafeString("%s/quality_flag",     GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    surface_flag    (info->reader->asset, info->reader->resource, SafeString("%s/surface_flag",     GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints)
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
Gedi02aReader::Gedi02a::~Gedi02a (void)
{
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi02aReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Gedi02aReader* reader =  (Gedi02aReader*)info->reader;
    GediParms* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "Gedi02a_reader", "{\"asset\":\"%s\", \"resource\":\"%s\", \"beam\":%d}", reader->asset->getName(), reader->resource, info->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        Region region(info);

        /* Read GEDI Datasets */
        Gedi02a Gedi02a(info, region);

        /* Increment Read Statistics */
        local_stats.footprints_read = region.num_footprints;

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; reader->active && footprint < region.num_footprints; footprint++)
        {
            /* Check Degrade Filter */
            if(parms->degrade_filter != GediParms::DEGRADE_UNFILTERED)
            {
                if(Gedi02a.degrade_flag[footprint] != parms->degrade_filter)
                {
                    local_stats.footprints_filtered++;
                    continue;
                }
            }

            /* Check L2 Quality Filter */
            if(parms->l2_quality_filter != GediParms::L2QLTY_UNFILTERED)
            {
                if(Gedi02a.quality_flag[footprint] != parms->l2_quality_filter)
                {
                    local_stats.footprints_filtered++;
                    continue;
                }
            }

            /* Check Surface Filter */
            if(parms->surface_filter != GediParms::SURFACE_UNFILTERED)
            {
                if(Gedi02a.surface_flag[footprint] != parms->surface_filter)
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
                fp->shot_number             = Gedi02a.shot_number[footprint];
                fp->time_ns                 = parms->deltatime2timestamp(Gedi02a.delta_time[footprint]);
                fp->latitude                = region.lat[footprint];
                fp->longitude               = region.lon[footprint];
                fp->elevation_lowestmode    = Gedi02a.elev_lowestmode[footprint];
                fp->elevation_highestreturn = Gedi02a.elev_highestreturn[footprint];
                fp->solar_elevation         = Gedi02a.solar_elevation[footprint];
                fp->sensitivity             = Gedi02a.sensitivity[footprint];
                fp->beam                    = info->beam;
                fp->flags                   = 0;
                if(Gedi02a.degrade_flag[footprint]) fp->flags |= GediParms::DEGRADE_FLAG_MASK;
                if(Gedi02a.quality_flag[footprint]) fp->flags |= GediParms::L2_QUALITY_FLAG_MASK;
                if(Gedi02a.surface_flag[footprint]) fp->flags |= GediParms::SURFACE_FLAG_MASK;

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
        mlog(e.level(), "Failure during processing of resource %s beam %d: %s", reader->resource, info->beam, e.what());
        LuaEndpoint::generateExceptionStatus(e.code(), e.level(), reader->outQ, &reader->active, "%s: (%s)", e.what(), reader->resource);
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Count Completion */
        reader->numComplete++;

        /* Send Final Record Batch */
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", info->reader->resource);
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
            if(reader->sendTerminator) reader->outQ->postCopy("", 0);
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
