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
#include "h5.h"
#include "gedi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi01bReader::fpRecType = "gedi01brec.footprint";
const RecordObject::fieldDef_t Gedi01bReader::fpRecDef[] = {
    {"shot_number",     RecordObject::UINT64,   offsetof(g01b_footprint_t, shot_number),        1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"time",            RecordObject::TIME8,    offsetof(g01b_footprint_t, time_ns),            1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"latitude",        RecordObject::DOUBLE,   offsetof(g01b_footprint_t, latitude),           1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(g01b_footprint_t, longitude),          1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"elevation_start", RecordObject::DOUBLE,   offsetof(g01b_footprint_t, elevation_start),    1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"elevation_stop",  RecordObject::DOUBLE,   offsetof(g01b_footprint_t, elevation_stop),     1,  NULL, NATIVE_FLAGS},
    {"solar_elevation", RecordObject::DOUBLE,   offsetof(g01b_footprint_t, solar_elevation),    1,  NULL, NATIVE_FLAGS},
    {"beam",            RecordObject::UINT8,    offsetof(g01b_footprint_t, beam),               1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT8,    offsetof(g01b_footprint_t, flags),              1,  NULL, NATIVE_FLAGS},
    {"tx_size",         RecordObject::UINT16,   offsetof(g01b_footprint_t, tx_size),            1,  NULL, NATIVE_FLAGS},
    {"rx_size",         RecordObject::UINT16,   offsetof(g01b_footprint_t, rx_size),            1,  NULL, NATIVE_FLAGS},
    {"tx_waveform",     RecordObject::FLOAT,    offsetof(g01b_footprint_t, tx_waveform),        G01B_MAX_TX_SAMPLES,  NULL, NATIVE_FLAGS},
    {"rx_waveform",     RecordObject::FLOAT,    offsetof(g01b_footprint_t, rx_waveform),        G01B_MAX_RX_SAMPLES,  NULL, NATIVE_FLAGS}
};

const char* Gedi01bReader::batchRecType = "gedi01brec";
const RecordObject::fieldDef_t Gedi01bReader::batchRecDef[] = {
    {"footprint",       RecordObject::USER,     offsetof(batch_t, footprint),         0,  fpRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

/******************************************************************************
 * GEDI L1B READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Gedi01bReader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    GediParms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = dynamic_cast<GediParms*>(getLuaObject(L, 4, GediParms::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Gedi01bReader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating Gedi01bReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Gedi01bReader::init (void)
{
    RECDEF(fpRecType,       fpRecDef,       sizeof(g01b_footprint_t),           NULL);
    RECDEF(batchRecType,    batchRecDef,    offsetof(batch_t, footprint[1]),    NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Gedi01bReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator):
    FootprintReader<g01b_footprint_t>(L, _asset, _resource,
                                      outq_name, _parms, _send_terminator,
                                      batchRecType, "geolocation/latitude_bin0", "geolocation/longitude_bin0",
                                      Gedi01bReader::subsettingThread)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::~Gedi01bReader (void) = default;

/*----------------------------------------------------------------------------
 * Gedi01b::Constructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Gedi01b::Gedi01b (const info_t* info, const Region& region):
    shot_number     (info->reader->context, FString("%s/shot_number",                  GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    delta_time      (info->reader->context, FString("%s/geolocation/delta_time",       GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    elev_bin0       (info->reader->context, FString("%s/geolocation/elevation_bin0",   GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    elev_lastbin    (info->reader->context, FString("%s/geolocation/elevation_lastbin",GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    solar_elevation (info->reader->context, FString("%s/geolocation/solar_elevation",  GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    degrade_flag    (info->reader->context, FString("%s/geolocation/degrade",          GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    tx_sample_count (info->reader->context, FString("%s/tx_sample_count",              GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    tx_start_index  (info->reader->context, FString("%s/tx_sample_start_index",        GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    rx_sample_count (info->reader->context, FString("%s/rx_sample_count",              GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints),
    rx_start_index  (info->reader->context, FString("%s/rx_sample_start_index",        GediParms::beam2group(info->beam)).c_str(), 0, region.first_footprint, region.num_footprints)
{
    /* Join Hardcoded Reads */
    shot_number.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    elev_bin0.join(info->reader->read_timeout_ms, true);
    elev_lastbin.join(info->reader->read_timeout_ms, true);
    solar_elevation.join(info->reader->read_timeout_ms, true);
    degrade_flag.join(info->reader->read_timeout_ms, true);
    tx_sample_count.join(info->reader->read_timeout_ms, true);
    tx_start_index.join(info->reader->read_timeout_ms, true);
    rx_sample_count.join(info->reader->read_timeout_ms, true);
    rx_start_index.join(info->reader->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Gedi01b::Destructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Gedi01b::~Gedi01b (void) = default;

/*----------------------------------------------------------------------------
 * subsetFootprints
 *----------------------------------------------------------------------------*/
void* Gedi01bReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Gedi01bReader* reader = reinterpret_cast<Gedi01bReader*>(info->reader);
    const GediParms* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "Gedi01b_reader", "{\"asset\":\"%s\", \"resource\":\"%s\", \"beam\":%d}", reader->asset->getName(), reader->resource, info->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read GEDI Datasets */
        const Gedi01b gedi01b(info, region);

        /* Read Waveforms */
        const long tx0 = gedi01b.tx_start_index[0] - 1;
        const long txN = gedi01b.tx_start_index[region.num_footprints - 1] - 1 + gedi01b.tx_sample_count[region.num_footprints - 1] - tx0;
        const long rx0 = gedi01b.rx_start_index[0] - 1;
        const long rxN = gedi01b.rx_start_index[region.num_footprints - 1] - 1 + gedi01b.rx_sample_count[region.num_footprints - 1] - rx0;
        H5Array<float> txwaveform(info->reader->context, FString("%s/txwaveform", GediParms::beam2group(info->beam)).c_str(), 0, tx0, txN);
        H5Array<float> rxwaveform(info->reader->context, FString("%s/rxwaveform", GediParms::beam2group(info->beam)).c_str(), 0, rx0, rxN);
        txwaveform.join(info->reader->read_timeout_ms, true);
        rxwaveform.join(info->reader->read_timeout_ms, true);

        /* Increment Read Statistics */
        local_stats.footprints_read = region.num_footprints;

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; reader->active && footprint < region.num_footprints; footprint++)
        {
            /* Check Degrade Filter */
            if(parms->degrade_filter != GediParms::DEGRADE_UNFILTERED)
            {
                if(gedi01b.degrade_flag[footprint] != parms->degrade_filter)
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
                g01b_footprint_t* fp        = &reader->batchData->footprint[reader->batchIndex];
                fp->shot_number             = gedi01b.shot_number[footprint];
                fp->time_ns                 = GediParms::deltatime2timestamp(gedi01b.delta_time[footprint]);
                fp->latitude                = region.lat[footprint];
                fp->longitude               = region.lon[footprint];
                fp->elevation_start         = gedi01b.elev_bin0[footprint];
                fp->elevation_stop          = gedi01b.elev_lastbin[footprint];
                fp->solar_elevation         = gedi01b.solar_elevation[footprint];
                fp->beam                    = info->beam;
                fp->flags                   = 0;
                fp->tx_size                 = gedi01b.tx_sample_count[footprint];
                fp->rx_size                 = gedi01b.rx_sample_count[footprint];;

                /* Set Flags */
                if(gedi01b.degrade_flag[footprint]) fp->flags |= GediParms::DEGRADE_FLAG_MASK;

                /* Populate Tx Waveform */
                const long tx_start_index = gedi01b.tx_start_index[footprint] - gedi01b.tx_start_index[0];
                const long tx_end_index = tx_start_index + MIN(fp->tx_size, G01B_MAX_TX_SAMPLES);
                for(long i = tx_start_index, j = 0; i < tx_end_index; i++, j++)
                {
                    fp->tx_waveform[j] = txwaveform[i];
                }

                /* Populate Rx Waveform */
                const long rx_start_index = gedi01b.rx_start_index[footprint] - gedi01b.rx_start_index[0];
                const long rx_end_index = rx_start_index + MIN(fp->rx_size, G01B_MAX_RX_SAMPLES);
                for(long i = rx_start_index, j = 0; i < rx_end_index; i++, j++)
                {
                    fp->rx_waveform[j] = rxwaveform[i];
                }

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
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s beam %d: %s", reader->resource, info->beam, e.what());
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
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, info->reader->resource);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", info->reader->resource);
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
