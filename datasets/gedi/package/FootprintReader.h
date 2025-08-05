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
#include "H5DArray.h"
#include "AncillaryFields.h"
#include "ContainerRecord.h"

#include "GediFields.h"

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

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

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
            char                group[9];
            GediFields::beam_t  beam;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                explicit Region     (const info_t* info);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (const info_t* info);
                void rasterregion   (const info_t* info);

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

        bool                    active;
        Thread*                 readerPid[GediFields::NUM_BEAMS];
        Mutex                   threadMut;
        int                     threadCount;
        int                     numComplete;
        bool                    sendTerminator;
        const int               read_timeout_ms;
        Publisher*              outQ;
        GediFields*             parms;
        stats_t                 stats;
        H5Coro::Context*        context;
        RecordObject            batchRecord;
        vector<RecordObject*>   ancRecords;
        int                     batchIndex;
        batch_t*                batchData;
        const char*             latName;
        const char*             lonName;
        Dictionary<H5DArray*>   ancData;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            FootprintReader         (lua_State* L, const char* outq_name, GediFields* _parms, bool _send_terminator,
                                                     const char* batch_rec_type, const char* lat_name, const char* lon_name,
                                                     subset_func_t subsetter);
                            ~FootprintReader        (void) override;
        bool                readAncillaryData       (const info_t* info, long first_footprint, long num_footprints);
        void                populateAncillaryFields (const info_t* info, long footprint, uint64_t shot_number);
        void                postRecordBatch         (stats_t* local_stats);
        static int          luaStats                (lua_State* L);
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

template <typename footprint_t>
const char* FootprintReader<footprint_t>::OBJECT_TYPE = "FootprintReader";

template <typename footprint_t>
const char* FootprintReader<footprint_t>::LUA_META_NAME = "FootprintReader";

template <typename footprint_t>
const struct luaL_Reg FootprintReader<footprint_t>::LUA_META_TABLE[] = {
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
FootprintReader<footprint_t>::FootprintReader ( lua_State* L, const char* outq_name, GediFields* _parms, bool _send_terminator,
                                                const char* batch_rec_type, const char* lat_name, const char* lon_name,
                                                subset_func_t subsetter ):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    parms(_parms),
    context(NULL),
    batchRecord(batch_rec_type, sizeof(batch_t)),
    latName(StringLib::duplicate(lat_name)),
    lonName(StringLib::duplicate(lon_name))
{
    assert(outq_name);
    assert(_parms);

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
    batchData = reinterpret_cast<batch_t*>(batchRecord.getRecordData());

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    threadCount = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Process Resource */
    try
    {
        /* Create H5Coro Context */
        context = new H5Coro::Context(parms->asset.asset, parms->getResource());

        /* Read Beam Data */
        threadMut.lock();
        {
            for(int i = 0; i < GediFields::NUM_BEAMS; i++)
            {
                if(parms->beams.enabled(i))
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    StringLib::format(info->group, 9, "%s", GediFields::beam2group(i));
                    convertFromIndex(i, info->beam);
                    readerPid[threadCount++] = new Thread(subsetter, info);
                }
            }
        }
        threadMut.unlock();

        if(threadCount == 0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "No valid beams specified, must be 0, 1, 2, 3, 5, 6, 8, 11, or -1 for all");
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, outQ, &active, "%s: (%s)", e.what(), parms->getResource());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, outQ, &active, "%s: (%s)", e.what(), parms->getResource());

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0, SYS_TIMEOUT);
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

    for(int i = 0; i < GediFields::NUM_BEAMS; i++)
    {
        if(readerPid[i]) delete readerPid[i];
    }

    delete [] latName;
    delete [] lonName;

    delete outQ;

    delete context;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
template <class footprint_t>
FootprintReader<footprint_t>::Region::Region (const info_t* info):
    lat             (info->reader->context, FString("%s/%s", info->group, info->reader->latName).c_str()),
    lon             (info->reader->context, FString("%s/%s", info->group, info->reader->lonName).c_str()),
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
    if(info->reader->parms->regionMask.valid())
    {
        rasterregion(info);
    }
    else if(info->reader->parms->pointsInPolygon.value > 0)
    {
        polyregion(info);
    }
    else
    {
        num_footprints = lat.size;
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
    delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::Region::polyregion (const info_t* info)
{
    /* Find First and Last Footprints in Polygon */
    bool first_footprint_found = false;
    int footprint = 0;
    while(footprint < lat.size)
    {
        /* Test Inclusion */
        const bool inclusion = info->reader->parms->polyIncludes(lon[footprint], lat[footprint]);

        /* Find First Footprint */
        if(!first_footprint_found && inclusion)
        {
            /* Set First Segment */
            first_footprint_found = true;
            first_footprint = footprint;
        }
        else if(first_footprint_found && !inclusion)
        {
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
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::Region::rasterregion (const info_t* info)
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
        const bool inclusion = info->reader->parms->maskIncludes(lon[footprint], lat[footprint]);
        inclusion_mask[footprint] = inclusion;

        /* If Coordinate Is In Raster */
        if(inclusion)
        {
            if(!first_footprint_found)
            {
                first_footprint_found = true;
                first_footprint = footprint;
            }
            last_footprint = footprint;
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
 * readAncillaryData
 *----------------------------------------------------------------------------*/
template <class footprint_t>
bool FootprintReader<footprint_t>::readAncillaryData (const info_t* info, long first_footprint, long num_footprints)
{
    vector<H5DArray*> arrays_to_join;

    /* Read Ancillary Fields */
    for(int i = 0; i < parms->anc_fields.length(); i++)
    {
        const FString dataset_name("%s/%s", info->group, parms->anc_fields[i].c_str());
        H5DArray* array = new H5DArray(context, dataset_name.c_str(), H5Coro::ALL_COLS, first_footprint, num_footprints);
        threadMut.lock();
        {
            const bool status = ancData.add(dataset_name.c_str(), array);
            if(!status) delete array;
            else arrays_to_join.push_back(array);
            assert(status); // the dictionary add should never fail
        }
        threadMut.unlock();
    }

    /* Join Ancillary Reads */
    bool status = true;
    for(H5DArray* array: arrays_to_join)
    {
        if(!array->join(read_timeout_ms, false))
        {
            status = false;
        }
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * populateAncillaryFields
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::populateAncillaryFields (const info_t* info, long footprint, uint64_t shot_number)
{
    (void)info;

    vector<AncillaryFields::field_t> field_vec;
    for(int i = 0; i < parms->anc_fields.length(); i++)
    {
        const FString dataset_name("%s/%s", info->group, parms->anc_fields[i].c_str());
        H5DArray* array = ancData[dataset_name.c_str()];

        if(array->numDimensions() > 1)
        {
            /* Create Element Array Records */
            const int record_size = offsetof(AncillaryFields::element_array_t, data) + (array->rowSize() * array->elementSize());
            RecordObject* element_array_rec = new RecordObject(AncillaryFields::ancElementRecType, record_size);
            AncillaryFields::element_array_t* data = reinterpret_cast<AncillaryFields::element_array_t*>(element_array_rec->getRecordData());

            data->extent_id = shot_number;
            data->anc_type = 0;
            data->field_index = i;
            data->data_type = array->elementType();
            data->num_elements = array->rowSize();
            array->serializeRow(&data->data[0], footprint);

            ancRecords.push_back(element_array_rec);
        }
        else
        {
            /* Create Field Array Record */
            AncillaryFields::field_t field;
            field.anc_type = 0;
            field.field_index = i;
            field.data_type = array->elementType();
            array->serializeRow(&field.value[0], footprint);
            field_vec.push_back(field);
        }
    }

    if(!field_vec.empty())
    {
        RecordObject* field_array_rec = AncillaryFields::createFieldArrayRecord(shot_number, field_vec); // memory allocation
        ancRecords.push_back(field_array_rec);
    }
}

/*----------------------------------------------------------------------------
 * postRecordBatch
 *----------------------------------------------------------------------------*/
template <class footprint_t>
void FootprintReader<footprint_t>::postRecordBatch (stats_t* local_stats)
{
    if(ancRecords.empty())
    {
        uint8_t* rec_buf = NULL;
        const int size = batchIndex * sizeof(footprint_t);
        const int rec_bytes = batchRecord.serialize(&rec_buf, RecordObject::REFERENCE, size);
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
    else
    {
        /* Trim Batch Record */
        const int size = batchIndex * sizeof(footprint_t);
        batchRecord.setUsedData(size);

        /* Build Vector of Records for Container Record */
        vector<RecordObject*> recvec;
        recvec.push_back(&batchRecord);
        for(auto rec: ancRecords) recvec.push_back(rec);

        /* Create and Serialize Container Record */
        uint8_t* rec_buf = NULL;
        ContainerRecord container(recvec);
        const int rec_bytes = container.serialize(&rec_buf, RecordObject::TAKE_OWNERSHIP);

        /* Post Record */
        int post_status = MsgQ::STATE_TIMEOUT;
        while(active && (post_status = outQ->postRef(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
        {
            local_stats->footprints_retried++;
        }

        /* Handle Success/Failure of Post */
        if(post_status > 0)
        {
            local_stats->footprints_sent++;
        }
        else
        {
            delete [] rec_buf; // delete buffer we now own and never was posted
            local_stats->footprints_dropped++;
        }

        /* Free Ancillary Records */
        for(size_t i = 0; i < ancRecords.size(); i++)
        {
            delete ancRecords[i];
        }

        /* Reset Vector of Ancillary Records */
        ancRecords.clear();
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
        lua_obj = reinterpret_cast<FootprintReader*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        const bool with_clear = getLuaBoolean(L, 2, true, false);

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
