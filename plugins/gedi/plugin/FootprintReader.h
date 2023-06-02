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

#ifndef __footprint_reader__
#define __footprint_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"
#include "H5Array.h"

#include "GediParms.h"

/******************************************************************************
 * FOOTPRINT READER
 ******************************************************************************/

template <class footprint_t>
class FootprintReader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int BATCH_SIZE = 256;

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Subsetting Function */
        typedef void* (*subset_func_t) (void* parm);

        /* Batch Record */
        typedef struct {
            footprint_t     footprint[BATCH_SIZE];
        } batch_t;

        /* Statistics */
        typedef struct {
            uint32_t        footprints_read;
            uint32_t        footprints_filtered;
            uint32_t        footprints_sent;
            uint32_t        footprints_dropped;
            uint32_t        footprints_retried;
        } stats_t;

        /* Thread Information */
        typedef struct {
            FootprintReader*    reader;
            int                 beam;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                Region              (info_t* info);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (info_t* info);
                void rasterregion   (info_t* info);

                H5Array<double>     lat;
                H5Array<double>     lon;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_footprint;
                long                num_footprints;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[GediParms::NUM_BEAMS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        Asset*              asset;
        const char*         resource;
        bool                sendTerminator;
        const int           read_timeout_ms;
        Publisher*          outQ;
        GediParms*          parms;
        stats_t             stats;
        H5Coro::context_t   context;
        RecordObject        batchRecord;
        int                 batchIndex;
        batch_t*            batchData;
        const char*         latName;
        const char*         lonName;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            FootprintReader         (lua_State* L, Asset* _asset, const char* _resource,
                                                     const char* outq_name, GediParms* _parms, bool _send_terminator,
                                                     const char* batch_rec_type, const char* lat_name, const char* lon_name,
                                                     subset_func_t subsetter);
                            ~FootprintReader        (void);
        void                postRecordBatch         (stats_t* local_stats);
        static int          luaStats                (lua_State* L);
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

template <typename footprint_t>
const char* FootprintReader<footprint_t>::OBJECT_TYPE = "FootprintReader";

template <typename footprint_t>
const char* FootprintReader<footprint_t>::LuaMetaName = "FootprintReader";

template <typename footprint_t>
const struct luaL_Reg FootprintReader<footprint_t>::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * GEDIT L4A READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class footprint_t>
FootprintReader<footprint_t>::FootprintReader ( lua_State* L, Asset* _asset, const char* _resource,
                                                const char* outq_name, GediParms* _parms, bool _send_terminator,
                                                const char* batch_rec_type, const char* lat_name, const char* lon_name,
                                                subset_func_t subsetter ):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    read_timeout_ms(_parms->read_timeout * 1000),
    batchRecord(batch_rec_type, sizeof(batch_t))
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;
    latName = StringLib::duplicate(lat_name);
    lonName = StringLib::duplicate(lon_name);

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
    batchData = (batch_t*)batchRecord.getRecordData();

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Count Number of Beams to Process */
    int beam_count = 0;
    int last_beam_index = -1;
    for(int i = 0; i < GediParms::NUM_BEAMS; i++)
    {
        if(parms->beams[i])
        {
            beam_count++;
            last_beam_index = i;
        }
    }

    /* Process Resource */
    try
    {
        /* Read Beam Data */
        if(beam_count == 1)
        {
            threadCount = 1;
            info_t* info = new info_t;
            info->reader = this;
            info->beam = GediParms::BEAM_NUMBER[last_beam_index];
            subsetter(info);
        }
        else if(beam_count > 0)
        {
            threadCount = beam_count;
            for(int i = 0; i < GediParms::NUM_BEAMS; i++)
            {
                if(parms->beams[i])
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    info->beam = GediParms::BEAM_NUMBER[i];
                    readerPid[i] = new Thread(subsetter, info);
                }
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "No valid beams specified, must be 0, 1, 2, 3, 5, 6, 8, 11, or -1 for all");
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
template <class footprint_t>
FootprintReader<footprint_t>::~FootprintReader (void)
{
    active = false;

    for(int i = 0; i < GediParms::NUM_BEAMS; i++)
    {
        if(readerPid[i]) delete readerPid[i];
    }

    delete [] latName;
    delete [] lonName;

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
template <class footprint_t>
FootprintReader<footprint_t>::Region::Region (info_t* info):
    lat             (info->reader->asset, info->reader->resource, SafeString("%s/%s", GediParms::beam2group(info->beam), info->reader->latName).str(), &info->reader->context),
    lon             (info->reader->asset, info->reader->resource, SafeString("%s/%s", GediParms::beam2group(info->beam), info->reader->lonName).str(), &info->reader->context),
    inclusion_mask  (NULL),
    inclusion_ptr   (NULL)
{
    /* Join Reads */
    lat.join(info->reader->read_timeout_ms, true);
    lon.join(info->reader->read_timeout_ms, true);

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
        num_footprints = MIN(lat.size, lon.size);
    }

    /* Check If Anything to Process */
    if(num_footprints <= 0)
    {
        cleanup();
        throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
    }

    /* Trim Geospatial Datasets Read from HDF5 File */
    lat.trim(first_footprint);
    lon.trim(first_footprint);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
template <class footprint_t>
FootprintReader<footprint_t>::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::Region::cleanup (void)
{
    if(inclusion_mask) delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::Region::polyregion (info_t* info)
{
    int points_in_polygon = info->reader->parms->polygon.length();

    /* Determine Best Projection To Use */
    MathLib::proj_t projection = MathLib::PLATE_CARREE;
    if(lat[0] > 70.0) projection = MathLib::NORTH_POLAR;
    else if(lat[0] < -70.0) projection = MathLib::SOUTH_POLAR;

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
    while(footprint < lat.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        MathLib::coord_t footprint_coord = {lon[footprint], lat[footprint]};
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
template <class footprint_t>
void FootprintReader<footprint_t>::Region::rasterregion (info_t* info)
{
    /* Allocate Inclusion Mask */
    if(lat.size <= 0) return;
    inclusion_mask = new bool [lat.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    bool first_footprint_found = false;
    long last_footprint = 0;
    int footprint = 0;
    while(footprint < lat.size)
    {
        /* Check Inclusion */
#warning FIX ME!!!
        double height = 0;
        bool inclusion = info->reader->parms->raster->includes(lon[footprint], lat[footprint], height);
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
 * postRecordBatch
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::postRecordBatch (stats_t* local_stats)
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
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
template <class footprint_t>
int FootprintReader<footprint_t>::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    FootprintReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (FootprintReader*)getLuaSelf(L, 1);
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
        LuaEngine::setAttrInt(L, "read",        lua_obj->stats.footprints_read);
        LuaEngine::setAttrInt(L, "filtered",    lua_obj->stats.footprints_filtered);
        LuaEngine::setAttrInt(L, "sent",        lua_obj->stats.footprints_sent);
        LuaEngine::setAttrInt(L, "dropped",     lua_obj->stats.footprints_dropped);
        LuaEngine::setAttrInt(L, "retried",     lua_obj->stats.footprints_retried);

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

#endif  /* __footprint_reader__ */
