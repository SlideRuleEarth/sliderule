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

#include "Hdf5Atl03Device.h"
#include "H5Array.h"
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE           "srt"
#define LUA_PARM_SIGNAL_CONFIDENCE      "cnf"
#define LUA_PARM_ALONG_TRACK_SPREAD     "ats"
#define LUA_PARM_PHOTON_COUNT           "cnt"
#define LUA_PARM_EXTENT_LENGTH          "len"
#define LUA_PARM_EXTENT_STEP            "res"

#define LUA_STAT_SEGMENTS_READ_L        "read_l"
#define LUA_STAT_SEGMENTS_READ_R        "read_r"
#define LUA_STAT_EXTENTS_FILTERED_L     "filtered_l"
#define LUA_STAT_EXTENTS_FILTERED_R     "filtered_r"
#define LUA_STAT_EXTENTS_ADDED          "added"
#define LUA_STAT_EXTENTS_SENT           "sent"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5Atl03Device::recType = "h5atl03";
const RecordObject::fieldDef_t Hdf5Atl03Device::recDef[] = {
    {"TRACK",       RecordObject::UINT8,    offsetof(extent_t, pair_reference_track),       sizeof(((extent_t*)0)->pair_reference_track),       NATIVE_FLAGS},
    {"SEG_ID",      RecordObject::UINT32,   offsetof(extent_t, segment_id),                 sizeof(((extent_t*)0)->segment_id),                 NATIVE_FLAGS},
    {"LENGTH",      RecordObject::DOUBLE,   offsetof(extent_t, length),                     sizeof(((extent_t*)0)->length),                     NATIVE_FLAGS},
    {"GPS_L",       RecordObject::DOUBLE,   offsetof(extent_t, gps_time[PRT_LEFT]),         sizeof(((extent_t*)0)->gps_time[PRT_LEFT]),         NATIVE_FLAGS},
    {"GPS_R",       RecordObject::DOUBLE,   offsetof(extent_t, gps_time[PRT_RIGHT]),        sizeof(((extent_t*)0)->gps_time[PRT_RIGHT]),        NATIVE_FLAGS},
    {"DIST_L",      RecordObject::DOUBLE,   offsetof(extent_t, start_distance[PRT_LEFT]),   sizeof(((extent_t*)0)->start_distance[PRT_LEFT]),   NATIVE_FLAGS},
    {"DIST_R",      RecordObject::DOUBLE,   offsetof(extent_t, start_distance[PRT_RIGHT]),  sizeof(((extent_t*)0)->start_distance[PRT_RIGHT]),  NATIVE_FLAGS},
    {"CNT_L",       RecordObject::UINT32,   offsetof(extent_t, photon_count[PRT_LEFT]),     sizeof(((extent_t*)0)->photon_count[PRT_LEFT]),     NATIVE_FLAGS},
    {"CNT_R",       RecordObject::UINT32,   offsetof(extent_t, photon_count[PRT_RIGHT]),    sizeof(((extent_t*)0)->photon_count[PRT_RIGHT]),    NATIVE_FLAGS},
    {"PHOTONS_L",   RecordObject::STRING,   offsetof(extent_t, photon_offset[PRT_LEFT]),    sizeof(((extent_t*)0)->photon_offset[PRT_LEFT]),    NATIVE_FLAGS | RecordObject::POINTER},
    {"PHOTONS_R",   RecordObject::STRING,   offsetof(extent_t, photon_offset[PRT_RIGHT]),   sizeof(((extent_t*)0)->photon_offset[PRT_RIGHT]),   NATIVE_FLAGS | RecordObject::POINTER}
};

const Hdf5Atl03Device::parms_t Hdf5Atl03Device::DefaultParms = {
    .surface_type = SRT_LAND_ICE,
    .signal_confidence = CNF_SURFACE_HIGH,
    .along_track_spread = 20.0, // meters
    .photon_count = 10, // PE
    .extent_length = 20.0, // meters
    .extent_step = 20.0
};

const double Hdf5Atl03Device::ATL03_SEGMENT_LENGTH = 20.0; // meters
const double Hdf5Atl03Device::MAX_ATL06_SEGMENT_LENGTH = 40.0; // meters

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<url>)
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::luaCreate (lua_State* L)
{
    try
    {
        /* Get URL */
        const char* url = getLuaString(L, 1);

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5Atl03Device(L, url));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating Hdf5Atl03Device: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5Atl03Device::Hdf5Atl03Device (lua_State* L, const char* url):
    DeviceObject(L, READER)
{
    /* Define Record */
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::defineRecord(recType, "TRACK", sizeof(extent_t), recDef, def_elements, 8);

    /* Set Parameters */
    parms = DefaultParms;

    /* Clear Statistics */
    LocalLib::set(&stats, 0, sizeof(stats));

    /* Set Segment List Index */
    listIndex = 0;

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s)", url, role == READER ? "READER" : "WRITER") + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s)", url, role == READER ? "READER" : "WRITER");

    /* Open URL */
    if(url[0])  connected = h5open(url);
    else        connected = false;

    /* Add Additional Meta Functions */
    LuaEngine::setAttrFunc(L, "config", luaConfig);
    LuaEngine::setAttrFunc(L, "parms",  luaParms);
    LuaEngine::setAttrFunc(L, "stats",  luaStats);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5Atl03Device::~Hdf5Atl03Device (void)
{
    if(config) delete [] config;

    for(int i = listIndex; i < extentList.length(); i++)
    {
        delete extentList[i];
    }
}

/*----------------------------------------------------------------------------
 * h5open
 *
 *  TODO: run this concurrent with readBuffer
 *  TODO: open and process all tracks in url
 *----------------------------------------------------------------------------*/
bool Hdf5Atl03Device::h5open (const char* url)
{
    bool status = false;
    int track = 1; // hardcode for now

    /* Open File */
    mlog(INFO, "Opening resource: %s\n", url);
    hid_t hid = H5Fopen(url, H5F_ACC_RDONLY, H5P_DEFAULT);
    if(hid >= 0)
    {
        try
        {
            /* Read Data from HDF5 File */
            H5Array<double>     sdp_gps_epoch   (hid, "/ancillary_data/atlas_sdp_gps_epoch");
            GTArray<double>     delta_time      (hid, track, "geolocation/delta_time");
            GTArray<int32_t>    segment_ph_cnt  (hid, track, "geolocation/segment_ph_cnt");
            GTArray<int32_t>    segment_id      (hid, track, "geolocation/segment_id");
            GTArray<double>     segment_dist_x  (hid, track, "geolocation/segment_dist_x");
            GTArray<float>      dist_ph_along   (hid, track, "heights/dist_ph_along");
            GTArray<float>      h_ph            (hid, track, "heights/h_ph");
            GTArray<char>       signal_conf_ph  (hid, track, "heights/signal_conf_ph", parms.surface_type);

            /* Initialize Dataset Scope Variables */
            int32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index
            int32_t seg_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // segment index
            int32_t cumulative_photons[PAIR_TRACKS_PER_GROUND_TRACK] = { segment_ph_cnt.gt[PRT_LEFT][0], segment_ph_cnt.gt[PRT_RIGHT][0] }; // total photons at start of segment

            /* Increment Read Statistics */
            stats.segments_read[PRT_LEFT] = segment_ph_cnt.gt[PRT_LEFT].size;
            stats.segments_read[PRT_RIGHT] = segment_ph_cnt.gt[PRT_RIGHT].size;

            /* Traverse All Photons In Dataset */
            while( (ph_in[PRT_LEFT] < dist_ph_along.gt[PRT_LEFT].size) &&
                   (ph_in[PRT_RIGHT] < dist_ph_along.gt[PRT_RIGHT].size) )
            {
                /* Determine Characteristics of Extent */
                int extent_size = sizeof(extent_t); // added onto below
                int32_t first_photon[PAIR_TRACKS_PER_GROUND_TRACK] = { ph_in[PRT_LEFT], ph_in[PRT_RIGHT] };
                int32_t next_photon[PAIR_TRACKS_PER_GROUND_TRACK] = { ph_in[PRT_LEFT], ph_in[PRT_RIGHT] };
                int32_t photon_count[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 };
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Traverse Photons Until Desired Along Track Distance Reached */
                    double along_track_distance = 0.0;
                    while(ph_in[t] < dist_ph_along.gt[t].size)
                    {
                        /* Update Along Track Distance */
                        along_track_distance += dist_ph_along.gt[t][ph_in[t]++];

                        /* Set Next Extent's First Photon to Inremented Photon Index */
                        if(along_track_distance < parms.extent_step)
                        {
                            next_photon[t] = ph_in[t];
                        }

                        /* Count Photon If Within Extent's Length */
                        if(along_track_distance < parms.extent_length)
                        {
                            photon_count[t]++;
                        }
                        else
                        {
                            break;
                        }
                    }

                    /* Find Next Extent's First Photon (if step > length) */
                    while(next_photon[t] < dist_ph_along.gt[t].size)
                    {
                        if(along_track_distance < parms.extent_step)
                        {
                            along_track_distance += dist_ph_along.gt[t][ph_in[t]++];
                            next_photon[t] = ph_in[t];
                        }
                    }

                    /* Check Photon Count */
                    if(photon_count[t] < parms.photon_count)
                    {
                        /* Filter Out Track's Segment */
                        photon_count[t] = 0;
                        stats.extents_filtered[t]++;
                    }

                    /* Check Along Track Spread */
                    int32_t last_photon = first_photon[t] + photon_count[t];
                    double along_track_spread = dist_ph_along.gt[t][last_photon] - dist_ph_along.gt[t][first_photon[t]];
                    if(along_track_spread < parms.along_track_spread)
                    {
                        /* Sanity Check Spread */
                        if(along_track_spread < 0.0)
                        {
                            mlog(WARNING, "Negative along track spread; spread=%lf, track=%d, photon_count=%d\n", along_track_spread, t, photon_count[t]);
                        }

                        /* Filter Out Track's Segment */
                        photon_count[t] = 0;
                        stats.extents_filtered[t]++;
                    }

                    /* Find Segment Id */
                    while(seg_in[t] < segment_id.gt[t].size)
                    {
                        int32_t next_cumulative_photons = cumulative_photons[t] + segment_ph_cnt.gt[t][seg_in[t]];
                        if(first_photon[t] >= next_cumulative_photons)
                        {
                            cumulative_photons[t] = next_cumulative_photons;
                            seg_in[t]++;
                        }
                        else
                        {
                            break;
                        }
                    }

                    /* Update Extent Size */
                    extent_size += sizeof(photon_t) * photon_count[t];

                    /* Update Photon Input Index to Next Extent */
                    ph_in[t] = next_photon[t];
                }

                /* Get Segment Index */
                int32_t start_seg = MIN(seg_in[PRT_LEFT], seg_in[PRT_RIGHT]);
                if(seg_in[PRT_LEFT] != seg_in[PRT_RIGHT])
                {
                    mlog(WARNING, "Segment index mismatch in %s for segments %d and %d\n", url, seg_in[PRT_LEFT], seg_in[PRT_RIGHT]);
                }

                /* Get Segment ID */
                int32_t seg_id = MIN(segment_id.gt[PRT_LEFT][start_seg], segment_id.gt[PRT_RIGHT][start_seg]);
                if(segment_id.gt[PRT_LEFT][start_seg] != segment_id.gt[PRT_RIGHT][start_seg])
                {
                    mlog(ERROR, "Segment ID mismatch in %s for segments %d and %d\n", url, segment_id.gt[PRT_LEFT][start_seg], segment_id.gt[PRT_RIGHT][start_seg]);
                }

                /* Allocate and Initialize Extent Record */
                RecordObject* record = new RecordObject(recType, extent_size); // overallocated memory... photons filtered below
                extent_t* extent = (extent_t*)record->getRecordData();
                extent->pair_reference_track = track;
                extent->segment_id = seg_id;
                extent->length = parms.extent_length;
                extent->gps_time[PRT_LEFT] = sdp_gps_epoch[0] + delta_time.gt[PRT_LEFT][start_seg];
                extent->gps_time[PRT_RIGHT] = sdp_gps_epoch[0] + delta_time.gt[PRT_RIGHT][start_seg];
                extent->start_distance[PRT_LEFT] = segment_dist_x.gt[PRT_LEFT][start_seg];
                extent->start_distance[PRT_RIGHT] = segment_dist_x.gt[PRT_RIGHT][start_seg];
                extent->photon_count[PRT_LEFT] = 0;
                extent->photon_count[PRT_RIGHT] = 0;

                /* Populate Extent Photons */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Loop Through Each Photon in Extent */
                    int32_t ph_in_seg_cnt = 0;
                    int32_t curr_seg = start_seg;
                    double delta_distance = 0.0;
                    for(int32_t p = first_photon[t]; p < (first_photon[t] + photon_count[t]); p++)
                    {
                        /* Calculate Delta Distance to Current Segment */
                        while(ph_in_seg_cnt >= segment_ph_cnt.gt[t][curr_seg])
                        {
                            ph_in_seg_cnt = 0; // reset photons in segment
                            curr_seg++; // go to next segment
                            delta_distance = segment_dist_x.gt[t][curr_seg] - segment_dist_x.gt[t][start_seg]; // calculate delta distance
                        }

                        /* Check Photon Signal Confidence Level */
                        if(signal_conf_ph.gt[t][p] >= parms.signal_confidence)
                        {
                            extent->photons[ph_out].distance_x = delta_distance + dist_ph_along.gt[t][p];
                            extent->photons[ph_out].height_y = h_ph.gt[t][p];
                            extent->photon_count[t]++;
                            ph_out++;
                        }

                        /* Count Photon in Segment */
                        ph_in_seg_cnt++;
                    }
                }

                /* Complete Extent Record */
                if(extent->photon_count[PRT_LEFT] != 0 || extent->photon_count[PRT_RIGHT] != 0)
                {
                    /* Set Photon Pointer Fields */
                    extent->photon_offset[PRT_LEFT] = sizeof(extent_t); // pointers are set to offset from start of record data
                    extent->photon_offset[PRT_RIGHT] = sizeof(extent_t) + (sizeof(photon_t) * extent->photon_count[PRT_LEFT]);

                    /* Resize Record Data */
                    int new_size = sizeof(extent_t) + (sizeof(photon_t) * (extent->photon_count[PRT_LEFT] + extent->photon_count[PRT_RIGHT]));
                    record->resizeData(new_size);

                    /* Add Segment Record */
                    extentList.add(record);
                    stats.extents_added++;
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
            mlog(CRITICAL, "Unable to process resource %s: %s\n", url, e.what());
        }

        /* Close File */
        H5Fclose(hid);
    }
    else
    {
        mlog(CRITICAL, "Failed to open resource: %s\n", url);
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool Hdf5Atl03Device::isConnected (int num_open)
{
    (void)num_open;

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Hdf5Atl03Device::closeConnection (void)
{
    connected = false;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::readBuffer (void* buf, int len)
{
    int bytes = TIMEOUT_RC;

    if(connected)
    {
        unsigned char* rec_buf = (unsigned char*)buf;

        /* Read Next Segment in List */
        if(listIndex < extentList.length())
        {
            RecordObject* record = extentList[listIndex];

            /* Check if Enough Room in Buffer to Hold Record */
            if(len >= record->getAllocatedMemory())
            {
                /* Serialize Record into Buffer */
                bytes = record->serialize(&rec_buf, RecordObject::COPY, len);
                stats.extents_sent++;
            }
            else
            {
                mlog(ERROR, "Unable to read ATL03 extent record, buffer too small (%d < %d)\n", len, record->getAllocatedMemory());
            }

            /* Release Segment Resources */
            delete record;

            /* Go To Next Segment */
            listIndex++;
        }
    }

    return bytes;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::getUniqueId (void)
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* Hdf5Atl03Device::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * luaConfig - :config(<{<key>=<value>, ...}>) --> success/failure
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::luaConfig (lua_State* L)
{
    bool status = false;
    Hdf5Atl03Device* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Hdf5Atl03Device*)getLuaSelf(L, 1);
    }
    catch(const LuaException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Check Table */
        if(lua_type(L, 2) != LUA_TTABLE)
        {
            throw LuaException("must supply table to configure %s", lua_obj->getName());
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

        lua_getfield(L, 2, LUA_PARM_EXTENT_LENGTH);
        lua_obj->parms.extent_length = (signalConf_t)getLuaFloat(L, -1, true, lua_obj->parms.extent_length);

        lua_getfield(L, 2, LUA_PARM_EXTENT_STEP);
        lua_obj->parms.extent_step = (signalConf_t)getLuaFloat(L, -1, true, lua_obj->parms.extent_step);

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", lua_obj->getName(), e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Hdf5Atl03Device* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Hdf5Atl03Device*)getLuaSelf(L, 1);
    }
    catch(const LuaException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_PARM_SURFACE_TYPE,         lua_obj->parms.surface_type);
        LuaEngine::setAttrInt(L, LUA_PARM_SIGNAL_CONFIDENCE,    lua_obj->parms.signal_confidence);
        LuaEngine::setAttrNum(L, LUA_PARM_ALONG_TRACK_SPREAD,   lua_obj->parms.along_track_spread);
        LuaEngine::setAttrInt(L, LUA_PARM_PHOTON_COUNT,         lua_obj->parms.photon_count);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_LENGTH,        lua_obj->parms.extent_length);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_STEP,          lua_obj->parms.extent_step);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning parameters %s: %s\n", lua_obj->getName(), e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Device::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Hdf5Atl03Device* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Hdf5Atl03Device*)getLuaSelf(L, 1);
    }
    catch(const LuaException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_L,      lua_obj->stats.segments_read[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_R,      lua_obj->stats.segments_read[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED_L,  lua_obj->stats.extents_filtered[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED_R,  lua_obj->stats.extents_filtered[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_ADDED,        lua_obj->stats.extents_added);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_SENT,        lua_obj->stats.extents_sent);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning stats %s: %s\n", lua_obj->getName(), e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
