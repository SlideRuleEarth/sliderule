/*
 * Copyright (c) 2025, University of Washington
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

#include "Gedi01bDataFrame.h"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi01bDataFrame::LUA_META_NAME = "Gedi01bDataFrame";
const struct luaL_Reg Gedi01bDataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * GEDI01B DATAFRAME CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<beam>, <parms>, <hdf01b>, <outq_name>)
 *----------------------------------------------------------------------------*/
int Gedi01bDataFrame::luaCreate (lua_State* L)
{
    GediFields* _parms = NULL;
    H5Object* _hdf01b = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<GediFields*>(getLuaObject(L, 2, GediFields::OBJECT_TYPE));
        _hdf01b = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return DataFrame Object */
        return createLuaObject(L, new Gedi01bDataFrame(L, beam_str, _parms, _hdf01b, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf01b) _hdf01b->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi01bDataFrame::Gedi01bDataFrame (lua_State* L, const char* beam_str, GediFields* _parms, H5Object* _hdf01b, const char* outq_name):
    GediDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"shot_number",         &shot_number},
        {"time_ns",             &time_ns},
        {"latitude",            &latitude},
        {"longitude",           &longitude},
        {"elevation_start",     &elevation_start},
        {"elevation_stop",      &elevation_stop},
        {"solar_elevation",     &solar_elevation},
        {"tx_size",             &tx_size},
        {"rx_size",             &rx_size},
        {"flags",               &flags},
        {"tx_waveform",         &tx_waveform},
        {"rx_waveform",         &rx_waveform}
    },
    _parms,
    _hdf01b,
    beam_str,
    outq_name)
{
    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Gedi01bData::Constructor
 *----------------------------------------------------------------------------*/
Gedi01bDataFrame::Gedi01bData::Gedi01bData (Gedi01bDataFrame* df, const AreaOfInterestGedi& aoi):
    shot_number     (df->hdf, FString("%s/shot_number",                   df->group).c_str(), 0, aoi.first_index, aoi.count),
    delta_time      (df->hdf, FString("%s/geolocation/delta_time",        df->group).c_str(), 0, aoi.first_index, aoi.count),
    elev_bin0       (df->hdf, FString("%s/geolocation/elevation_bin0",    df->group).c_str(), 0, aoi.first_index, aoi.count),
    elev_lastbin    (df->hdf, FString("%s/geolocation/elevation_lastbin", df->group).c_str(), 0, aoi.first_index, aoi.count),
    solar_elevation (df->hdf, FString("%s/geolocation/solar_elevation",   df->group).c_str(), 0, aoi.first_index, aoi.count),
    degrade_flag    (df->hdf, FString("%s/geolocation/degrade",           df->group).c_str(), 0, aoi.first_index, aoi.count),
    tx_sample_count (df->hdf, FString("%s/tx_sample_count",               df->group).c_str(), 0, aoi.first_index, aoi.count),
    tx_start_index  (df->hdf, FString("%s/tx_sample_start_index",         df->group).c_str(), 0, aoi.first_index, aoi.count),
    rx_sample_count (df->hdf, FString("%s/rx_sample_count",               df->group).c_str(), 0, aoi.first_index, aoi.count),
    rx_start_index  (df->hdf, FString("%s/rx_sample_start_index",         df->group).c_str(), 0, aoi.first_index, aoi.count),
    anc_data        (df->parms->anc_fields, df->hdf, df->group, H5Coro::ALL_COLS, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    shot_number.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    elev_bin0.join(df->readTimeoutMs, true);
    elev_lastbin.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    degrade_flag.join(df->readTimeoutMs, true);
    tx_sample_count.join(df->readTimeoutMs, true);
    tx_start_index.join(df->readTimeoutMs, true);
    rx_sample_count.join(df->readTimeoutMs, true);
    rx_start_index.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi01bDataFrame::subsettingThread (void* parm)
{
    Gedi01bDataFrame* df = static_cast<Gedi01bDataFrame*>(parm);
    const GediFields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "gedi01b_dataframe", "{\"context\":\"%s\", \"beam\":%s}", df->hdf->name, df->beamStr);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Area of Interest */
        const AreaOfInterestGedi aoi(df->hdf, df->group, "geolocation/latitude_bin0", "geolocation/longitude_bin0", df->parms, df->readTimeoutMs);

        /* Read GEDI Datasets */
        const Gedi01bData gedi01b(df, aoi);

        /* Read Waveforms */
        const long tx0 = gedi01b.tx_start_index[0] - 1;
        const long txN = gedi01b.tx_start_index[aoi.count - 1] - 1 + gedi01b.tx_sample_count[aoi.count - 1] - tx0;
        const long rx0 = gedi01b.rx_start_index[0] - 1;
        const long rxN = gedi01b.rx_start_index[aoi.count - 1] - 1 + gedi01b.rx_sample_count[aoi.count - 1] - rx0;
        H5Array<float> txwaveform(df->hdf, FString("%s/txwaveform", df->group).c_str(), 0, tx0, txN);
        H5Array<float> rxwaveform(df->hdf, FString("%s/rxwaveform", df->group).c_str(), 0, rx0, rxN);
        txwaveform.join(df->readTimeoutMs, true);
        rxwaveform.join(df->readTimeoutMs, true);

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; df->active.load() && footprint < aoi.count; footprint++)
        {
            /* Check Degrade Filter */
            if(parms.degrade_filter)
            {
                if(gedi01b.degrade_flag[footprint])
                {
                    continue;
                }
            }

            /* Check Region */
            if(aoi.inclusion_ptr)
            {
                if(!aoi.inclusion_ptr[footprint])
                {
                    continue;
                }
            }

            /* Add Row */
            df->addRow();

            /* Populate Columns */
            df->shot_number.append(gedi01b.shot_number[footprint]);
            df->time_ns.append(GediFields::deltatime2timestamp(gedi01b.delta_time[footprint]));
            df->latitude.append(aoi.latitude[footprint]);
            df->longitude.append(aoi.longitude[footprint]);
            df->elevation_start.append(static_cast<float>(gedi01b.elev_bin0[footprint]));
            df->elevation_stop.append(gedi01b.elev_lastbin[footprint]);
            df->solar_elevation.append(static_cast<double>(gedi01b.solar_elevation[footprint]));
            df->tx_size.append(gedi01b.tx_sample_count[footprint]);
            df->rx_size.append(gedi01b.rx_sample_count[footprint]);

            uint8_t row_flags = 0;
            if(gedi01b.degrade_flag[footprint]) row_flags |= GediFields::DEGRADE_FLAG_MASK;
            df->flags.append(row_flags);

            /* Populate Tx Waveform */
            FieldList<float> tx_list(GEDI01B_TX_SAMPLES_MAX, 0.0);
            const long tx_start = gedi01b.tx_start_index[footprint] - gedi01b.tx_start_index[0];
            const long tx_end = tx_start + MIN(gedi01b.tx_sample_count[footprint], static_cast<uint16_t>(GEDI01B_TX_SAMPLES_MAX));
            for(long i = tx_start, j = 0; i < tx_end; i++, j++)
            {
                tx_list[j] = txwaveform[i];
            }
            df->tx_waveform.append(tx_list);

            /* Populate Rx Waveform */
            FieldList<float> rx_list(GEDI01B_RX_SAMPLES_MAX, 0.0);
            const long rx_start = gedi01b.rx_start_index[footprint] - gedi01b.rx_start_index[0];
            const long rx_end = rx_start + MIN(gedi01b.rx_sample_count[footprint], static_cast<uint16_t>(GEDI01B_RX_SAMPLES_MAX));
            for(long i = rx_start, j = 0; i < rx_end; i++, j++)
            {
                rx_list[j] = rxwaveform[i];
            }
            df->rx_waveform.append(rx_list);

            /* Ancillary Data */
            if(gedi01b.anc_data.length() > 0)
            {
                gedi01b.anc_data.addToGDF(df, footprint);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf->name, df->beamStr, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
