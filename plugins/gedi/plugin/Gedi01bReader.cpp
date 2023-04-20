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
 * DEFINES
 ******************************************************************************/

#define LUA_STAT_FOOTPRINTS_READ        "read"
#define LUA_STAT_FOOTPRINTS_FILTERED    "filtered"
#define LUA_STAT_FOOTPRINTS_SENT        "sent"
#define LUA_STAT_FOOTPRINTS_DROPPED     "dropped"
#define LUA_STAT_FOOTPRINTS_RETRIED     "retried"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi01bReader::fpRecType = "gedi01brec.footprint";
const RecordObject::fieldDef_t Gedi01bReader::fpRecDef[] = {
    {"shot_number",     RecordObject::UINT64,   offsetof(footprint_t, shot_number),         1,  NULL, NATIVE_FLAGS},
    {"time",            RecordObject::TIME8,    offsetof(footprint_t, time_ns),             1,  NULL, NATIVE_FLAGS},
    {"latitude",        RecordObject::DOUBLE,   offsetof(footprint_t, latitude),            1,  NULL, NATIVE_FLAGS},
    {"longitude",       RecordObject::DOUBLE,   offsetof(footprint_t, longitude),           1,  NULL, NATIVE_FLAGS},
    {"elevation_start", RecordObject::DOUBLE,   offsetof(footprint_t, elevation_start),     1,  NULL, NATIVE_FLAGS},
    {"elevation_stop",  RecordObject::DOUBLE,   offsetof(footprint_t, elevation_stop),      1,  NULL, NATIVE_FLAGS},
    {"solar_elevation", RecordObject::DOUBLE,   offsetof(footprint_t, solar_elevation),     1,  NULL, NATIVE_FLAGS},
    {"beam",            RecordObject::UINT8,    offsetof(footprint_t, beam),                1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT8,    offsetof(footprint_t, flags),               1,  NULL, NATIVE_FLAGS},
    {"tx_size",         RecordObject::UINT16,   offsetof(footprint_t, tx_size),             1,  NULL, NATIVE_FLAGS},
    {"rx_size",         RecordObject::UINT16,   offsetof(footprint_t, rx_size),             1,  NULL, NATIVE_FLAGS},
    {"tx_waveform",     RecordObject::FLOAT,    offsetof(footprint_t, tx_waveform),         MAX_TX_SAMPLES,  NULL, NATIVE_FLAGS},
    {"rx_waveform",     RecordObject::FLOAT,    offsetof(footprint_t, rx_waveform),         MAX_RX_SAMPLES,  NULL, NATIVE_FLAGS}
};

const char* Gedi01bReader::batchRecType = "gedi01brec";
const RecordObject::fieldDef_t Gedi01bReader::batchRecDef[] = {
    {"footprint",       RecordObject::USER,     offsetof(gedi02a_t, footprint),         0,  fpRecType, NATIVE_FLAGS}
};

const char* Gedi01bReader::OBJECT_TYPE = "Gedi01bReader";
const char* Gedi01bReader::LuaMetaName = "Gedi01bReader";
const struct luaL_Reg Gedi01bReader::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * GEDIT L4A READER CLASS
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
        asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = (GediParms*)getLuaObject(L, 4, GediParms::OBJECT_TYPE);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

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
    RECDEF(fpRecType,       fpRecDef,       sizeof(footprint_t),                NULL);
    RECDEF(batchRecType,    batchRecDef,    offsetof(gedi02a_t, footprint[1]),  NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Gedi01bReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    read_timeout_ms(_parms->read_timeout * 1000),
    batchRecord(batchRecType, sizeof(gedi02a_t))
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.footprints_read       = 0;
    stats.footprints_filtered   = 0;
    stats.footprints_sent       = 0;
    stats.footprints_dropped    = 0;
    stats.footprints_retried    = 0;

    /* Initialize Batch Record Processing */
    batchIndex = 0;
    batchData = (gedi02a_t*)batchRecord.getRecordData();

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* PRocess Resource */
    try
    {
        /* Read Beam Data */
        if(parms->beam == GediParms::ALL_BEAMS)
        {
            threadCount = GediParms::NUM_BEAMS;
            for(int b = 0; b < GediParms::NUM_BEAMS; b++)
            {
                info_t* info = new info_t;
                info->reader = this;
                info->beam = GediParms::BEAM_NUMBER[b];
                readerPid[b] = new Thread(subsettingThread, info);
            }
        }
        else if(parms->beam == GediParms::BEAM0000 ||
                parms->beam == GediParms::BEAM0001 ||
                parms->beam == GediParms::BEAM0010 ||
                parms->beam == GediParms::BEAM0011 ||
                parms->beam == GediParms::BEAM0101 ||
                parms->beam == GediParms::BEAM0110 ||
                parms->beam == GediParms::BEAM1000 ||
                parms->beam == GediParms::BEAM1011)
        {
            threadCount = 1;
            info_t* info = new info_t;
            info->reader = this;
            info->beam = parms->beam;
            subsettingThread(info);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid beam specified <%d>, must be 0, 1, 2, 3, 5, 6, 8, 11, or -1 for all", parms->beam);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Log Error */
        mlog(e.level(), "Failed to process resource %s: %s", resource, e.what());

        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) LuaEndpoint::generateExceptionStatus(RTE_TIMEOUT, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);
        else LuaEndpoint::generateExceptionStatus(RTE_RESOURCE_DOES_NOT_EXIST, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::~Gedi01bReader (void)
{
    active = false;

    for(int b = 0; b < GediParms::NUM_BEAMS; b++)
    {
        if(readerPid[b]) delete readerPid[b];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Region::Region (info_t* info):
    lat_bin0        (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/latitude_bin0", GediParms::beam2group(info->beam)).str(), &info->reader->context),
    lon_bin0        (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/longitude_bin0", GediParms::beam2group(info->beam)).str(), &info->reader->context),
    inclusion_mask  (NULL),
    inclusion_ptr   (NULL)
{
    /* Join Reads */
    lat_bin0.join(info->reader->read_timeout_ms, true);
    lon_bin0.join(info->reader->read_timeout_ms, true);

    /* Initialize Region */
    first_footprint = 0;
    num_footprints = H5Coro::ALL_ROWS;

    /* Determine Spatial Extent */
    if(info->reader->parms->raster != NULL)
    {
        rasterregion(info);
    }
    else if(info->reader->parms->polygon.length() > 0)
    {
        polyregion(info);
    }
    else
    {
        num_footprints = MIN(lat_bin0.size, lon_bin0.size);
    }

    /* Check If Anything to Process */
    if(num_footprints <= 0)
    {
        cleanup();
        throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
    }

    /* Trim Geospatial Datasets Read from HDF5 File */
    lat_bin0.trim(first_footprint);
    lon_bin0.trim(first_footprint);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Gedi01bReader::Region::cleanup (void)
{
    if(inclusion_mask) delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Gedi01bReader::Region::polyregion (info_t* info)
{
    int points_in_polygon = info->reader->parms->polygon.length();

    /* Determine Best Projection To Use */
    MathLib::proj_t projection = MathLib::PLATE_CARREE;
    if(lat_bin0[0] > 70.0) projection = MathLib::NORTH_POLAR;
    else if(lat_bin0[0] < -70.0) projection = MathLib::SOUTH_POLAR;

    /* Project Polygon */
    List<MathLib::coord_t>::Iterator poly_iterator(info->reader->parms->polygon);
    MathLib::point_t* projected_poly = new MathLib::point_t [points_in_polygon];
    for(int i = 0; i < points_in_polygon; i++)
    {
        projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
    }

    /* Find First and Last Footprints in Polygon */
    bool first_footprint_found = false;
    bool last_footprint_found = false;
    int footprint = 0;
    while(footprint < lat_bin0.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        MathLib::coord_t footprint_coord = {lon_bin0[footprint], lat_bin0[footprint]};
        MathLib::point_t footprint_point = MathLib::coord2point(footprint_coord, projection);

        /* Test Inclusion */
        if(MathLib::inpoly(projected_poly, points_in_polygon, footprint_point))
        {
            inclusion = true;
        }

        /* Find First Footprint */
        if(!first_footprint_found && inclusion)
        {
            /* Set First Segment */
            first_footprint_found = true;
            first_footprint = footprint;
        }
        else if(first_footprint_found && !last_footprint_found && !inclusion)
        {
            /* Set Last Segment */
            last_footprint_found = true;
            break; // full extent found!
        }

        /* Bump Footprint */
        footprint++;
    }

    /* Set Number of Segments */
    if(first_footprint_found)
    {
        num_footprints = footprint - first_footprint;
    }

    /* Delete Projected Polygon */
    delete [] projected_poly;
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void Gedi01bReader::Region::rasterregion (info_t* info)
{
    /* Allocate Inclusion Mask */
    if(lat_bin0.size <= 0) return;
    inclusion_mask = new bool [lat_bin0.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    bool first_footprint_found = false;
    long last_footprint = 0;
    int footprint = 0;
    while(footprint < lat_bin0.size)
    {
        /* Check Inclusion */
        bool inclusion = info->reader->parms->raster->includes(lon_bin0[footprint], lat_bin0[footprint]);
        inclusion_mask[footprint] = inclusion;

        /* If Coordinate Is In Raster */
        if(inclusion)
        {
            if(!first_footprint_found)
            {
                first_footprint_found = true;
                first_footprint = footprint;
                last_footprint = footprint;
            }
            else
            {
                last_footprint = footprint;
            }
        }

        /* Bump Footprint */
        footprint++;
    }

    /* Set Number of Footprints */
    if(first_footprint_found)
    {
        num_footprints = last_footprint - first_footprint + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_footprint];
    }
}

/*----------------------------------------------------------------------------
 * Gedi01b::Constructor
 *----------------------------------------------------------------------------*/
Gedi01bReader::Gedi01b::Gedi01b (info_t* info, Region& region):
    shot_number     (info->reader->asset, info->reader->resource, SafeString("%s/shot_number",                  GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    delta_time      (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/delta_time",       GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    elev_bin0       (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/elevation_bin0",   GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    elev_lastbin    (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/elevation_lastbin",GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    solar_elevation (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/solar_elevation",  GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    degrade_flag    (info->reader->asset, info->reader->resource, SafeString("%s/geolocation/degrade",          GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    tx_sample_count (info->reader->asset, info->reader->resource, SafeString("%s/tx_sample_count",              GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    tx_start_index  (info->reader->asset, info->reader->resource, SafeString("%s/tx_sample_start_index",        GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    rx_sample_count (info->reader->asset, info->reader->resource, SafeString("%s/rx_sample_count",              GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints),
    rx_start_index  (info->reader->asset, info->reader->resource, SafeString("%s/rx_sample_start_index",        GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, region.first_footprint, region.num_footprints)
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
Gedi01bReader::Gedi01b::~Gedi01b (void)
{
}

/*----------------------------------------------------------------------------
 * postRecordBatch
 *----------------------------------------------------------------------------*/
void Gedi01bReader::postRecordBatch (stats_t* local_stats)
{
    uint8_t* rec_buf = NULL;
    int size = batchIndex * sizeof(footprint_t);
    int rec_bytes = batchRecord.serialize(&rec_buf, RecordObject::REFERENCE, size);
    int post_status = MsgQ::STATE_TIMEOUT;
    while( active && ((post_status = outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT) );
    if(post_status > 0)
    {
        local_stats->footprints_sent += batchIndex;
    }
    else
    {
        mlog(ERROR, "Failed to post %s to stream %s: %d", batchRecord.getRecordType(), outQ->getName(), post_status);
        local_stats->footprints_dropped += batchIndex;
    }
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi01bReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Gedi01bReader* reader = info->reader;
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
        Gedi01b gedi01b(info, region);

        /* Read Waveforms */
        long tx0 = gedi01b.tx_start_index[0] - 1;
        long txN = gedi01b.tx_start_index[region.num_footprints - 1] - 1 + gedi01b.tx_sample_count[region.num_footprints - 1] - tx0;
        long rx0 = gedi01b.rx_start_index[0] - 1;
        long rxN = gedi01b.rx_start_index[region.num_footprints - 1] - 1 + gedi01b.rx_sample_count[region.num_footprints - 1] - rx0;
        H5Array<float> txwaveform(info->reader->asset, info->reader->resource, SafeString("%s/txwaveform", GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, tx0, txN);
        H5Array<float> rxwaveform(info->reader->asset, info->reader->resource, SafeString("%s/rxwaveform", GediParms::beam2group(info->beam)).str(), &info->reader->context, 0, rx0, txN);
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
                footprint_t* fp = &reader->batchData->footprint[reader->batchIndex];
                fp->shot_number             = gedi01b.shot_number[footprint];
                fp->time_ns                 = parms->deltatime2timestamp(gedi01b.delta_time[footprint]);
                fp->latitude                = region.lat_bin0[footprint];
                fp->longitude               = region.lon_bin0[footprint];
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
                long tx_start_index = gedi01b.tx_start_index[footprint] - gedi01b.tx_start_index[0];
                long tx_end_index = tx_start_index + MIN(fp->tx_size, MAX_TX_SAMPLES);
                for(long i = tx_start_index, j = 0; i < tx_end_index; i++, j++)
                {
                    fp->tx_waveform[j] = txwaveform[i];
                }

                /* Populate Rx Waveform */
                long rx_start_index = gedi01b.rx_start_index[footprint] - gedi01b.rx_start_index[0];
                long rx_end_index = rx_start_index + MIN(fp->rx_size, MAX_RX_SAMPLES);
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

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Gedi01bReader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Gedi01bReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Gedi01bReader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_READ,            lua_obj->stats.footprints_read);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_FILTERED,     lua_obj->stats.footprints_filtered);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_SENT,         lua_obj->stats.footprints_sent);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_DROPPED,      lua_obj->stats.footprints_dropped);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_RETRIED,      lua_obj->stats.footprints_retried);

        /* Clear if Requested */
        if(with_clear) memset(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
