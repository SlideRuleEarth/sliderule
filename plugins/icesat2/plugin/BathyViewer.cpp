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

#include "core.h"
#include "h5.h"
#include "icesat2.h"
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
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <parms>)
 *----------------------------------------------------------------------------*/
int BathyViewer::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    Icesat2Parms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = getLuaString(L, 2);
        parms = dynamic_cast<Icesat2Parms*>(getLuaObject(L, 3, Icesat2Parms::OBJECT_TYPE));

        /* Return Reader Object */
        return createLuaObject(L, new BathyViewer(L, asset, resource, parms));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating BathyViewer: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void BathyViewer::init (void)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyViewer::BathyViewer (lua_State* L, Asset* _asset, const char* _resource, Icesat2Parms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->read_timeout * 1000),
    bathyMask(NULL),
    totalPhotons(0),
    totalPhotonsInMask(0),
    totalSegments(0),
    totalSegmentsInMask(0),
    totalErrors(0)
{
    assert(_asset);
    assert(_resource);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;
    bathyMask = new GeoLib::TIFFImage(NULL, GLOBAL_BATHYMETRY_MASK_FILE_PATH);

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Read Global Resource Information */
    try
    {
        /* Count Threads */
        for(int track = 1; track <= Icesat2Parms::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < Icesat2Parms::NUM_PAIR_TRACKS; pair++)
            {
                const int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams[gt_index] && (parms->track == Icesat2Parms::ALL_TRACKS || track == parms->track))
                {
                    threadCount++;
                }
            }
        }

        /* Check if Readers Created */
        if(threadCount == 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "No reader threads were created, invalid track specified: %d\n", parms->track);
        }

        /* Create Readers */
        int pid_cnt = 0;
        for(int track = 1; track <= Icesat2Parms::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < Icesat2Parms::NUM_PAIR_TRACKS; pair++)
            {
                const int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams[gt_index] && (parms->track == Icesat2Parms::ALL_TRACKS || track == parms->track))
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    info->track = track;
                    info->pair = pair;
                    StringLib::format(info->prefix, 7, "/gt%d%c", info->track, info->pair == 0 ? 'l' : 'r');
                    readerPid[pid_cnt++] = new Thread(subsettingThread, info);
                }
            }
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        mlog(e.level(), "Failure on resource %s: %s", resource, e.what());

        /* Indicate End of Data */
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyViewer::~BathyViewer (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete bathyMask;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
BathyViewer::Region::Region (info_t* info):
    segment_lat    (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str(), &info->reader->context),
    segment_lon    (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str(), &info->reader->context),
    segment_ph_cnt (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str(), &info->reader->context)
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
        while(reader->active && (current_segment < region.segment_ph_cnt.size))
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
            const uint32_t pixel = reader->bathyMask->getPixel(x, y);
            if(pixel != GLOBAL_BATHYMETRY_MASK_OFF_VALUE)
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
        mlog(e.level(), "Failure on resource %s track %d.%d: %s", reader->resource, info->track, info->pair, e.what());
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
            mlog(INFO, "Completed processing resource %s: %ld photons", reader->resource, reader->totalPhotons);
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

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "total_photons", lua_obj->totalPhotons);
        LuaEngine::setAttrInt(L, "photons_in_mask", lua_obj->totalPhotonsInMask);
        LuaEngine::setAttrInt(L, "total_segments", lua_obj->totalSegments);
        LuaEngine::setAttrInt(L, "segments_in_mask", lua_obj->totalSegmentsInMask);
        LuaEngine::setAttrInt(L, "total_errors", lua_obj->totalErrors);

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
