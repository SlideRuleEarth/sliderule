/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

#include "Hdf5Atl03Handle.h"
#include "H5Array.h"
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE           "srt"
#define LUA_PARM_SIGNAL_CONFIDENCE      "cnf"
#define LUA_PARM_ALONG_TRACK_SPREAD     "ats"
#define LUA_PARM_PHOTON_COUNT           "cnt"
#define LUA_PARM_SEGMENT_LENGTH         "res"

#define LUA_STAT_SEGMENTS_READ_L        "read_l"
#define LUA_STAT_SEGMENTS_READ_R        "read_r"
#define LUA_STAT_SEGMENTS_FILTERED_L    "filtered_l"
#define LUA_STAT_SEGMENTS_FILTERED_R    "filtered_r"
#define LUA_STAT_SEGMENTS_ADDED         "added"
#define LUA_STAT_SEGMENTS_SENT          "sent"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5Atl03Handle::LuaMetaName = "Hdf5Atl03Handle";
const struct luaL_Reg Hdf5Atl03Handle::LuaMetaTable[] = {
    {"config",      luaConfig},
    {"parms",       luaParms},
    {"stats",       luaStats},
    {NULL,          NULL}
};

const char* Hdf5Atl03Handle::recType = "h5atl03";
const RecordObject::fieldDef_t Hdf5Atl03Handle::recDef[] = {
    {"TRACK",       RecordObject::UINT8,    offsetof(segment_t, track),                     sizeof(((segment_t*)0)->track),                     NATIVE_FLAGS},
    {"SEGMENT_ID",  RecordObject::UINT32,   offsetof(segment_t, segment_id),                sizeof(((segment_t*)0)->segment_id),                NATIVE_FLAGS},
    {"SEGMENT_LEN", RecordObject::DOUBLE,   offsetof(segment_t, segment_length),            sizeof(((segment_t*)0)->segment_length),            NATIVE_FLAGS},
    {"PHOTONS_L",   RecordObject::STRING,   offsetof(segment_t, photon_offset[PRT_LEFT]),   sizeof(((segment_t*)0)->photon_offset[PRT_LEFT]),   NATIVE_FLAGS | RecordObject::POINTER},
    {"PHOTONS_R",   RecordObject::STRING,   offsetof(segment_t, photon_offset[PRT_RIGHT]),  sizeof(((segment_t*)0)->photon_offset[PRT_RIGHT]),  NATIVE_FLAGS | RecordObject::POINTER},
    {"CNT_L",       RecordObject::UINT32,   offsetof(segment_t, photon_count[PRT_LEFT]),    sizeof(((segment_t*)0)->photon_count[PRT_LEFT]),    NATIVE_FLAGS},
    {"CNT_R",       RecordObject::UINT32,   offsetof(segment_t, photon_count[PRT_RIGHT]),   sizeof(((segment_t*)0)->photon_count[PRT_RIGHT]),   NATIVE_FLAGS}
};

const Hdf5Atl03Handle::parms_t Hdf5Atl03Handle::DefaultParms = {
    .surface_type = SRT_LAND_ICE,
    .signal_confidence = CNF_SURFACE_HIGH,
    .along_track_spread = 20.0, // meters
    .photon_count = 10, // PE
    .segment_length = 40.0 // meters
};

const double Hdf5Atl03Handle::ATL03_SEGMENT_LENGTH = 20.0; // meters
const double Hdf5Atl03Handle::MAX_ATL06_SEGMENT_LENGTH = 40.0; // meters

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<track>)
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int track = getLuaInteger(L, 1);

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5Atl03Handle(L, track));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5Atl03Handle::Hdf5Atl03Handle (lua_State* L, int _track):
    Hdf5Handle(L, LuaMetaName, LuaMetaTable)
{
    /* Define Record */
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::defineRecord(recType, "TRACK", sizeof(segment_t), recDef, def_elements, 8);

    /* Set Ground Reference Track */
    track = _track;

    /* Set Parameters */
    parms = DefaultParms;

    /* Clear Statistics */
    LocalLib::set(&stats, 0, sizeof(stats));

    /* Set Segment List Index */
    listIndex = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5Atl03Handle::~Hdf5Atl03Handle (void)
{
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
bool Hdf5Atl03Handle::open (const char* filename, DeviceObject::role_t role)
{
    unsigned flags;
    bool status = false;

    /* Set Flags */
    if(role == DeviceObject::READER)        flags = H5F_ACC_RDONLY;
    else if(role == DeviceObject::WRITER)   flags = H5F_ACC_TRUNC;
    else                                    flags = H5F_ACC_RDWR;

    /* Open File */
    mlog(INFO, "Opening file: %s\n", filename);
    hid_t file = H5Fopen(filename, flags, H5P_DEFAULT);
    if(file >= 0)
    {
        try
        {
            /* Read Data from HDF5 File */
            GTArray<int32_t>    segment_ph_cnt  (file, track, "geolocation/segment_ph_cnt");
            GTArray<int32_t>    segment_id      (file, track, "geolocation/segment_id");
            GTArray<double>     segment_dist_x  (file, track, "geolocation/segment_dist_x");
            GTArray<float>      dist_ph_along   (file, track, "heights/dist_ph_along");
            GTArray<float>      h_ph            (file, track, "heights/h_ph");
            GTArray<char>       signal_conf_ph  (file, track, "heights/signal_conf_ph", parms.surface_type);

            /* Initialize Dataset Scope Variables */
            uint32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index
            uint32_t ph_seg_start_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index

            /* Increment Read Statistics */
            stats.segments_read[PRT_LEFT] = segment_ph_cnt.gt[PRT_LEFT].size;
            stats.segments_read[PRT_RIGHT] = segment_ph_cnt.gt[PRT_RIGHT].size;

            /* Get Number of Segments */
            int num_segs = MIN(segment_ph_cnt.gt[PRT_LEFT].size, segment_ph_cnt.gt[PRT_RIGHT].size);
            if(segment_ph_cnt.gt[PRT_LEFT].size != segment_ph_cnt.gt[PRT_RIGHT].size)
            {
                mlog(WARNING, "Segment size mismatch in %s for track %d\n", filename, track);
            }

            /* Go Through Each Segment in File */
            for(int s = 0; s < num_segs; s++)
            {
                /* Get Segment ID */
                uint32_t seg_id = segment_id.gt[PRT_LEFT][s];
                if(segment_id.gt[PRT_LEFT][s] != segment_id.gt[PRT_RIGHT][s])
                {
                    mlog(WARNING, "Segment ID mismatch in %s for segments %d and %d\n", filename, segment_id.gt[PRT_LEFT][s], segment_id.gt[PRT_RIGHT][s]);
                }

                /* Obtain Extent of Segment */
                int32_t photon_count[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 };
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    int p = ph_seg_start_in[t];
                    double along_track_distance = dist_ph_along.gt[t][p];

                    while(along_track_distance < parms.segment_length && p < dist_ph_along.gt[t].size)
                    {

                    }
                }


                /* Allocate Segment Data */
                int seg_size = sizeof(segment_t) + (sizeof(photon_t) * (segment_ph_cnt.gt[PRT_LEFT][s] + segment_ph_cnt.gt[PRT_RIGHT][s]));
                RecordObject* record = new RecordObject(recType, seg_size); // overallocated memory... photons filtered below
                segment_t* segment = (segment_t*)record->getRecordData();

                /* Initialize Attributes of Segment */
                segment->track = track;
                segment->segment_id = seg_id;
                segment->photon_count[PRT_LEFT] = 0;
                segment->photon_count[PRT_RIGHT] = 0;

                /* Populate Segment Record Photons */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Check Photon Count */
                    int32_t photon_count = segment_ph_cnt.gt[t][s];
                    if((photon_count < parms.photon_count) || (photon_count <= 0))
                    {
                        /* Filter Out Track's Segment */
                        photon_count = 0;
                        stats.segments_filtered[t]++;
                    }

                    /* Check Along Track Spread */
                    int32_t first_photon = ph_in[t];
                    int32_t last_photon = ph_in[t] + photon_count - 1;
                    if(photon_count > 0)
                    {
                        double along_track_spread = dist_ph_along.gt[t][last_photon] - dist_ph_along.gt[t][first_photon];
                        if(along_track_spread < parms.along_track_spread)
                        {
                            /* Sanity Check Spread */
                            if(along_track_spread < 0.0)
                            {
                                mlog(WARNING, "Negative along track spread; spread=%lf, track=%d, photon_count=%d\n", along_track_spread, t, photon_count);
                            }

                            /* Filter Out Track's Segment */
                            photon_count = 0;
                            last_photon = first_photon - 1;
                            stats.segments_filtered[t]++;
                        }
                    }

                    /* Get Along Track Distance */
                    double along_track_distance = segment_dist_x.gt[t][s];
                    if(s > 1) printf("DELTA: %lf\n", along_track_distance - segment_dist_x.gt[t][s-1]);
                    // TODO: rework this function so that it uses the segment_length to determine what goes into a segment.
                    // each segment is about 20.0xxx meters... each photon will need to have a current along track distance value
                    // that fits within the segment length... then the actual distance_x will need to be an offset from the first
                    // ATL03 segment's sigment_dist_x value
                    // also... for segment lengths < 20.0, there will need to be an additional ID in the record to identify it
                    // as well as the along_track_distance for the segment (or maybe just that)

                    /* Loop Through Each Photon in Segment */
                    for(int32_t p = first_photon; p <= last_photon; p++)
                    {
                        if(signal_conf_ph.gt[t][ph_in[t]] >= parms.signal_confidence)
                        {
                            segment->photons[ph_out].distance_x = dist_ph_along.gt[t][p] /* + segment_dist_x.gt[t][s] */;
                            segment->photons[ph_out].height_y = h_ph.gt[t][p];
                            segment->photon_count[t]++;
                            ph_out++;
                        }
                    }

                    /* Update Photon Index */
                    ph_in[t] += segment_ph_cnt.gt[t][s];
                }

                if(segment->photon_count[PRT_LEFT] != 0 || segment->photon_count[PRT_RIGHT] != 0)
                {
                    /* Set Photon Pointer Fields */
                    segment->photon_offset[PRT_LEFT] = sizeof(segment_t); // pointers are set to offset from start of record data
                    segment->photon_offset[PRT_RIGHT] = sizeof(segment_t) + (sizeof(photon_t) * segment->photon_count[PRT_LEFT]);

                    /* Resize Record Data */
                    int new_size = sizeof(segment_t) + (sizeof(photon_t) * (segment->photon_count[PRT_LEFT] + segment->photon_count[PRT_RIGHT]));
                    record->resizeData(new_size);

                    /* Add Segment Record */
                    segmentList.add(record);
                    stats.segments_added++;
                }
                else
                {
                    /* Free Resources Associated with Record */
                    delete record;
                }
            }

            /* Set Success */
            status = true;
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Unable to process file %s: %s\n", filename, e.what());
        }

        /* Close File */
        H5Fclose(file);
    }
    else
    {
        mlog(CRITICAL, "Failed to open file: %s\n", filename);
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::read (void* buf, int len)
{
    int bytes_read = 0;
    unsigned char* rec_buf = (unsigned char*)buf;

    /* Read Next Segment in List */
    if(listIndex < segmentList.length())
    {
        RecordObject* record = segmentList[listIndex];

        /* Check if Enough Room in Buffer to Hold Record */
        if(len >= record->getAllocatedMemory())
        {
            /* Serialize Record into Buffer */
            bytes_read = record->serialize(&rec_buf, RecordObject::COPY, len);
            stats.segments_sent++;
        }
        else
        {
            mlog(ERROR, "Unable to read ATL03 segment record, buffer too small (%d < %d)\n", len, record->getAllocatedMemory());
        }

        /* Release Segment Resources */
        delete record;

        /* Go To Next Segment */
        listIndex++;
    }

    /* Return Bytes Read */
    return bytes_read;
}

/*----------------------------------------------------------------------------
 * write
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::write (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return 0;
}

/*----------------------------------------------------------------------------
 * close
 *----------------------------------------------------------------------------*/
void Hdf5Atl03Handle::close (void)
{
}

/*----------------------------------------------------------------------------
 * luaConfig - :config(<{<key>=<value>, ...}>) --> success/failure
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaConfig (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Hdf5Atl03Handle* lua_obj = (Hdf5Atl03Handle*)getLuaSelf(L, 1);

        /* Check Table */
        if(lua_type(L, 2) != LUA_TTABLE)
        {
            throw LuaException("must supply table to configure %s", LuaMetaName);
        }

        /* Get Configuration Parameters from Table */
        lua_getfield(L, 2, LUA_PARM_SURFACE_TYPE);
        lua_obj->parms.surface_type = (surfaceType_t)getLuaInteger(L, -1, true, lua_obj->parms.surface_type);

        lua_getfield(L, 2, LUA_PARM_SIGNAL_CONFIDENCE);
        lua_obj->parms.signal_confidence = (signalConf_t)getLuaInteger(L, -1, true, lua_obj->parms.signal_confidence);

        lua_getfield(L, 2, LUA_PARM_ALONG_TRACK_SPREAD);
        lua_obj->parms.along_track_spread = (signalConf_t)getLuaFloat(L, -1, true, lua_obj->parms.along_track_spread);

        lua_getfield(L, 2, LUA_PARM_PHOTON_COUNT);
        lua_obj->parms.photon_count = (signalConf_t)getLuaInteger(L, -1, true, lua_obj->parms.photon_count);

        lua_getfield(L, 2, LUA_PARM_SEGMENT_LENGTH);
        lua_obj->parms.segment_length = (signalConf_t)getLuaFloat(L, -1, true, lua_obj->parms.segment_length);

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Hdf5Atl03Handle* lua_obj = (Hdf5Atl03Handle*)getLuaSelf(L, 1);

        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_PARM_SURFACE_TYPE,         lua_obj->parms.surface_type);
        LuaEngine::setAttrInt(L, LUA_PARM_SIGNAL_CONFIDENCE,    lua_obj->parms.signal_confidence);
        LuaEngine::setAttrNum(L, LUA_PARM_ALONG_TRACK_SPREAD,   lua_obj->parms.along_track_spread);
        LuaEngine::setAttrInt(L, LUA_PARM_PHOTON_COUNT,         lua_obj->parms.photon_count);
        LuaEngine::setAttrNum(L, LUA_PARM_SEGMENT_LENGTH,       lua_obj->parms.segment_length);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning parameters %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Hdf5Atl03Handle* lua_obj = (Hdf5Atl03Handle*)getLuaSelf(L, 1);

        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_L,      lua_obj->stats.segments_read[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_R,      lua_obj->stats.segments_read[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_FILTERED_L,  lua_obj->stats.segments_filtered[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_FILTERED_R,  lua_obj->stats.segments_filtered[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_ADDED,       lua_obj->stats.segments_added);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_SENT,        lua_obj->stats.segments_sent);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning stats %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
