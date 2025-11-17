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
#include "LuaObject.h"
#include "H5Object.h"
#include "Icesat2Fields.h"
#include "Atl13DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl13DataFrame::LUA_META_NAME = "Atl13DataFrame";
const struct luaL_Reg Atl13DataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Atl13DataFrame::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;
    H5Object* _hdf13 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        _hdf13 = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return Reader Object */
        return createLuaObject(L, new Atl13DataFrame(L, beam_str, _parms, _hdf13, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf13) _hdf13->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl13DataFrame::Atl13DataFrame (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf13, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",                 &time_ns},
        {"latitude",                &latitude},
        {"longitude",               &longitude},
        {"ht_ortho",                &ht_ortho},
        {"ht_water_surf",           &ht_water_surf},
        {"stdev_water_surf",        &stdev_water_surf},
        {"water_depth",             &water_depth},
    },
    {
        {"spot",                    &spot},
        {"cycle",                   &cycle},
        {"rgt",                     &rgt},
        {"gt",                      &gt},
        {"granule",                 &granule}
    },
    Icesat2Fields::defaultEGM(_parms->granuleFields.version.value)),
    spot(0, META_COLUMN),
    cycle(_parms->granuleFields.cycle.value, META_COLUMN),
    rgt(_parms->granuleFields.rgt.value, META_COLUMN),
    gt(0, META_COLUMN),
    granule(_hdf13->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    beam(FString("%s", beam_str).c_str(true)),
    outQ(NULL),
    parms(_parms),
    hdf13(_hdf13)
{
    assert(_parms);
    assert(_hdf13);

    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Calculate Key */
    dfKey = 0;
    const int exp_beam_str_len = 4;
    for(int i = 0; i < exp_beam_str_len; i++)
    {
        if(beam[i] == '\0') break;
        dfKey += static_cast<int>(beam[i]);
    }

    /* Setup Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl13DataFrame::~Atl13DataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete [] beam;
    delete outQ;
    parms->releaseLuaObject();
    hdf13->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Atl13DataFrame::getKey(void) const
{
    return dfKey;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::Constructor
 *----------------------------------------------------------------------------*/
Atl13DataFrame::AreaOfInterest::AreaOfInterest (const Atl13DataFrame* df):
    useRefId        (df->parms->atl13.reference_id.value > 0),
    atl13refid      (useRefId ? df->hdf13 : NULL, FString("%s/%s", df->beam, "atl13refid").c_str()),
    latitude        (df->hdf13, FString("%s/%s", df->beam, "segment_lat").c_str()),
    longitude       (df->hdf13, FString("%s/%s", df->beam, "segment_lon").c_str()),
    inclusionMask   {NULL},
    inclusionPtr    {NULL}
{
    try
    {
        /* Initialize Segment Range */
        lastSegment = -1;
        firstSegment = 0;
        numSegments = -1;

        /* Perform Initial Reference ID Search */
        if(useRefId)
        {
            /* Join ATL13 Reference ID Read */
            atl13refid.join(df->readTimeoutMs, true);

            bool first_segment_found = false;
            for(long i = 0; i < atl13refid.size; i++)
            {
                if(atl13refid[i] == df->parms->atl13.reference_id.value)
                {
                    if(!first_segment_found)
                    {
                        first_segment_found = true;
                        firstSegment = i;
                    }
                    lastSegment = i;
                }
            }

            /* Calculate Initial Number of Segments */
            numSegments = lastSegment - firstSegment + 1;
            if(numSegments <= 0) throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "reference id not found");
        }

        /* Join Latitude/Longitude Reads */
        latitude.join(df->readTimeoutMs, true);
        longitude.join(df->readTimeoutMs, true);

        /* Determine Spatial Extent */
        if(df->parms->regionMask.valid())
        {
            rasterregion(df);
        }
        else if(df->parms->pointsInPolygon.value > 0)
        {
            polyregion(df);
        }

        /* Check If Anything to Process */
        if(numSegments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        latitude.trim(firstSegment);
        longitude.trim(firstSegment);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::Destructor
 *----------------------------------------------------------------------------*/
Atl13DataFrame::AreaOfInterest::~AreaOfInterest (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::cleanup
 *----------------------------------------------------------------------------*/
void Atl13DataFrame::AreaOfInterest::cleanup (void)
{
    delete [] inclusionMask;
    inclusionMask = NULL;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::polyregion
 *----------------------------------------------------------------------------*/
void Atl13DataFrame::AreaOfInterest::polyregion (const Atl13DataFrame* df)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = firstSegment;
    const int max_segment = (numSegments < 0) ? longitude.size : numSegments;
    while(segment < max_segment)
    {
        /* Test Inclusion */
        const bool inclusion = df->parms->polyIncludes(longitude[segment], latitude[segment]);

        /* Check First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Polygon */
            first_segment_found = true;
            firstSegment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* If Coordinate Is NOT In Polygon */
            break; // full extent found!
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        numSegments = segment - firstSegment;
    }
    else
    {
        numSegments = 0;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::rasterregion
 *----------------------------------------------------------------------------*/
void Atl13DataFrame::AreaOfInterest::rasterregion (const Atl13DataFrame* df)
{
    /* Find First Segment In Raster */
    bool first_segment_found = false;

    /* Allocate Inclusion Mask */
    inclusionMask = new bool [numSegments];
    inclusionPtr = inclusionMask;

    /* Loop Throuh Segments */
    int segment = firstSegment;
    const int max_segment = (numSegments < 0) ? longitude.size : numSegments;
    while(segment < max_segment)
    {
        /* Check Inclusion */
        const bool inclusion = df->parms->maskIncludes(longitude[segment], latitude[segment]);
        inclusionMask[segment] = inclusion;
        if(inclusion)
        {
            if(!first_segment_found)
            {
                first_segment_found = true;
                firstSegment = segment;
            }
            lastSegment = segment;
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        numSegments = lastSegment - firstSegment + 1;

        /* Trim Inclusion Mask */
        inclusionPtr = &inclusionMask[firstSegment];
    }
    else
    {
        numSegments = 0;
    }
}

/*----------------------------------------------------------------------------
 * Atl13Data::Constructor
 *----------------------------------------------------------------------------*/
Atl13DataFrame::Atl13Data::Atl13Data (Atl13DataFrame* df, const AreaOfInterest& aoi):
    sc_orient               (df->hdf13,                            "/orbit_info/sc_orient"),
    delta_time              (df->hdf13, FString("%s/%s", df->beam, "delta_time").c_str(),               0, aoi.firstSegment, aoi.numSegments),
    ht_ortho                (df->hdf13, FString("%s/%s", df->beam, "ht_ortho").c_str(),                 0, aoi.firstSegment, aoi.numSegments),
    ht_water_surf           (df->hdf13, FString("%s/%s", df->beam, "ht_water_surf").c_str(),            0, aoi.firstSegment, aoi.numSegments),
    stdev_water_surf        (df->hdf13, FString("%s/%s", df->beam, "stdev_water_surf").c_str(),         0, aoi.firstSegment, aoi.numSegments),
    water_depth             (df->hdf13, FString("%s/%s", df->beam, "water_depth").c_str(),              0, aoi.firstSegment, aoi.numSegments),
    anc_data                (df->parms->atl13.anc_fields, df->hdf13, FString("%s", df->beam).c_str(),   0, aoi.firstSegment, aoi.numSegments)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    ht_ortho.join(df->readTimeoutMs, true);
    ht_water_surf.join(df->readTimeoutMs, true);
    stdev_water_surf.join(df->readTimeoutMs, true);
    water_depth.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl13DataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    Atl13DataFrame* df = static_cast<Atl13DataFrame*>(parm);
//    const Icesat2Fields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "atl13_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf13->name, df->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to AreaOfInterest of Interest */
        const AreaOfInterest aoi(df);

        /* Read ATL03 Datasets */
        const Atl13Data atl13(df, aoi);

        /* Set MetaData */
        df->spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl13.sc_orient[0], df->beam);
        df->gt = Icesat2Fields::getGroundTrack(df->beam);

        /* Traverse All Photons In Dataset */
        int32_t current_segment = -1;
        while(df->active.load() && (++current_segment < aoi.numSegments))
        {
            /* Check AreaOfInterest Mask */
            if(aoi.inclusionPtr)
            {
                if(!aoi.inclusionPtr[current_segment])
                {
                    continue;
                }
            }

            /* Add Photon to DataFrame */
            df->addRow();
            df->ht_ortho.append(atl13.ht_ortho[current_segment]);
            df->ht_water_surf.append(atl13.ht_water_surf[current_segment]);
            df->time_ns.append(Icesat2Fields::deltatime2timestamp(atl13.delta_time[current_segment]));
            df->latitude.append(aoi.latitude[current_segment]);
            df->longitude.append(aoi.longitude[current_segment]);
            df->stdev_water_surf.append(atl13.stdev_water_surf[current_segment]);
            df->water_depth.append(atl13.water_depth[current_segment]);

            /* Add Ancillary Elements */
            if(atl13.anc_data.length() > 0) atl13.anc_data.addToGDF(df, current_segment);
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf13->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
