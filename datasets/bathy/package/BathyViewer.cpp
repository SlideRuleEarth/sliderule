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

#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdarg.h>

#include "icesat2.h"
#include "OsApi.h"
#include "H5Coro.h"
#include "H5Array.h"
#include "GeoLib.h"
#include "RasterObject.h"

#include "BathyViewer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyViewer::GLOBAL_BATHYMETRY_MASK_FILE_PATH = "/data/ATL24_Mask_v5_Raster.tif";
const double BathyViewer::GLOBAL_BATHYMETRY_MASK_MAX_LAT = 84.25;
const double BathyViewer::GLOBAL_BATHYMETRY_MASK_MIN_LAT = -79.0;
const double BathyViewer::GLOBAL_BATHYMETRY_MASK_MAX_LON = 180.0;
const double BathyViewer::GLOBAL_BATHYMETRY_MASK_MIN_LON = -180.0;
const double BathyViewer::GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE = 0.25;
const uint32_t BathyViewer::GLOBAL_BATHYMETRY_MASK_OFF_VALUE = 0xFFFFFFFF;

const char* BathyViewer::OBJECT_TYPE = "BathyViewer";
const char* BathyViewer::LUA_META_NAME = "BathyViewer";
const struct luaL_Reg BathyViewer::LUA_META_TABLE[] = {
    {"counts",      luaCounts},
    {NULL,          NULL}
};

/******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <parms>)
 *----------------------------------------------------------------------------*/
int BathyViewer::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));

        /* Return Reader Object */
        return createLuaObject(L, new BathyViewer(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating BathyViewer: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyViewer::BathyViewer (lua_State* L, Icesat2Fields* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    context(NULL),
    bathyMask(NULL),
    totalPhotons(0),
    totalPhotonsInMask(0),
    totalSegments(0),
    totalSegmentsInMask(0),
    totalErrors(0)
{
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    parms = _parms;
    bathyMask = new GeoLib::TIFFImage(NULL, GLOBAL_BATHYMETRY_MASK_FILE_PATH);

    /* Initialize Readers */
    active.store(true);
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Read Global Resource Information */
    try
    {
        /* Create H5Coro Context */
        context = new H5Coro::Context(parms->asset.asset, parms->getResource());

        /* Create Readers */
        threadMut.lock();
        {
            for(int track = 1; track <= Icesat2Fields::NUM_TRACKS; track++)
            {
                for(int pair = 0; pair < Icesat2Fields::NUM_PAIR_TRACKS; pair++)
                {
                    const int gt_index = (2 * (track - 1)) + pair;
                    if(parms->beams.values[gt_index] && (parms->track == Icesat2Fields::ALL_TRACKS || track == parms->track))
                    {
                        info_t* info = new info_t;
                        info->reader = this;
                        info->track = track;
                        info->pair = pair;
                        StringLib::format(info->prefix, 7, "/gt%d%c", info->track, info->pair == 0 ? 'l' : 'r');
                        readerPid[threadCount++] = new Thread(subsettingThread, info);
                    }
                }
            }
        }
        threadMut.unlock();

        /* Check if Readers Created */
        if(threadCount == 0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "No reader threads were created, invalid track specified: %d\n", parms->track.value);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        mlog(e.level(), "Failure on resource %s: %s", parms->getResource(), e.what());

        /* Indicate End of Data */
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyViewer::~BathyViewer (void)
{
    active.store(false);

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete bathyMask;

    delete context;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
BathyViewer::Region::Region (info_t* info):
    segment_lat    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str()),
    segment_lon    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str())
{
    /* Join Reads */
    segment_lat.join(info->reader->read_timeout_ms, true);
    segment_lon.join(info->reader->read_timeout_ms, true);
    segment_ph_cnt.join(info->reader->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* BathyViewer::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = static_cast<info_t*>(parm);
    BathyViewer* reader = info->reader;

    /* Initialize Count of Photons */
    int64_t total_photons = 0;
    int64_t photons_in_mask = 0;
    int64_t total_segments = 0;
    int64_t segments_in_mask = 0;
    int64_t total_errors = 0;

    try
    {
        /* Region of Interest */
        const Region region(info);

        /* Count Total Segments */
        total_segments = region.segment_ph_cnt.size;

        /* Traverse All Segments In Dataset */
        int32_t current_segment = 0; // index into the segment rate variables
        while(reader->active.load() && (current_segment < region.segment_ph_cnt.size))
        {
            /* Get Y Coordinate */
            const double degrees_of_latitude = region.segment_lat[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LAT;
            const double latitude_pixels = degrees_of_latitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
            const uint32_t y = static_cast<uint32_t>(latitude_pixels);

            /* Get X Coordinate */
            const double degrees_of_longitude =  region.segment_lon[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LON;
            const double longitude_pixels = degrees_of_longitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
            const uint32_t x = static_cast<uint32_t>(longitude_pixels);

            /* Get Photons in Segment */
            int32_t photons_in_segment = region.segment_ph_cnt[current_segment];
            if(photons_in_segment < MIN_PH_IN_SEG || photons_in_segment > MAX_PH_IN_SEG)
            {
                photons_in_segment = 0; // zero out count since it is out of bounds
                total_errors++;
            }

            /* Count Photons in Mask*/
            const GeoLib::TIFFImage::val_t pixel = reader->bathyMask->getPixel(x, y);
            if(pixel.u32 == GLOBAL_BATHYMETRY_MASK_OFF_VALUE)
            {
                photons_in_mask += photons_in_segment;
                segments_in_mask++;
            }

            /* Count Total Photons */
            total_photons += photons_in_segment;

            /* Goto Next Segment */
            current_segment++;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure on resource %s track %d.%d: %s", reader->parms->getResource(), info->track, info->pair, e.what());
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Sum Total */
        reader->totalPhotons += total_photons;
        reader->totalPhotonsInMask += photons_in_mask;
        reader->totalSegments += total_segments;
        reader->totalSegmentsInMask += segments_in_mask;
        reader->totalErrors += total_errors;

        /* Count Completion */
        reader->numComplete++;
        if(reader->numComplete == reader->threadCount)
        {
            /* Indicate End of Data */
            mlog(INFO, "Completed processing resource %s: %ld photons", reader->parms->getResource(), reader->totalPhotons);
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up */
    delete info;

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaCounts - :counts()
 *----------------------------------------------------------------------------*/
int BathyViewer::luaCounts (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    BathyViewer* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<BathyViewer*>(getLuaSelf(L, 1));

        /* Snapshot counts under lock to avoid races with worker threads */
        lua_obj->threadMut.lock();
        const int64_t total_photons = lua_obj->totalPhotons;
        const int64_t photons_in_mask = lua_obj->totalPhotonsInMask;
        const int64_t total_segments = lua_obj->totalSegments;
        const int64_t segments_in_mask = lua_obj->totalSegmentsInMask;
        const int64_t total_errors = lua_obj->totalErrors;
        lua_obj->threadMut.unlock();

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "total_photons", total_photons);
        LuaEngine::setAttrInt(L, "photons_in_mask", photons_in_mask);
        LuaEngine::setAttrInt(L, "total_segments", total_segments);
        LuaEngine::setAttrInt(L, "segments_in_mask", segments_in_mask);
        LuaEngine::setAttrInt(L, "total_errors", total_errors);

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
