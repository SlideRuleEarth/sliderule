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
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5Atl03Handle::LuaMetaName = "Hdf5Atl03Handle";
const struct luaL_Reg Hdf5Atl03Handle::LuaMetaTable[] = {
    {"config",      luaConfig},
    {"parms",       luaParms},
    {NULL,          NULL}
};

const char* Hdf5Atl03Handle::recType = "h5atl03";
const RecordObject::fieldDef_t Hdf5Atl03Handle::recDef[] = {
    {"TRACK",       RecordObject::UINT8,    offsetof(segment_t, track),                     sizeof(((segment_t*)0)->track),                     NATIVE_FLAGS},
    {"SEGMENT_ID",  RecordObject::UINT32,   offsetof(segment_t, segment_id),                sizeof(((segment_t*)0)->segment_id),                NATIVE_FLAGS},
    {"PHOTONS_L",   RecordObject::STRING,   offsetof(segment_t, photon_offset[PRT_LEFT]),   sizeof(((segment_t*)0)->photon_offset[PRT_LEFT]),   NATIVE_FLAGS | RecordObject::POINTER},
    {"PHOTONS_R",   RecordObject::STRING,   offsetof(segment_t, photon_offset[PRT_RIGHT]),  sizeof(((segment_t*)0)->photon_offset[PRT_RIGHT]),  NATIVE_FLAGS | RecordObject::POINTER},
    {"NUM_L",       RecordObject::UINT32,   offsetof(segment_t, num_photons[PRT_LEFT]),     sizeof(((segment_t*)0)->num_photons[PRT_LEFT]),     NATIVE_FLAGS},
    {"NUM_R",       RecordObject::UINT32,   offsetof(segment_t, num_photons[PRT_RIGHT]),    sizeof(((segment_t*)0)->num_photons[PRT_RIGHT]),    NATIVE_FLAGS}
};

const Hdf5Atl03Handle::parms_t Hdf5Atl03Handle::DefaultParms = {
    .srt = SRT_LAND_ICE,
    .cnf = CNF_SURFACE_HIGH
};

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
            GTArray<char>       signal_conf_ph  (file, track, "heights/signal_conf_ph", parms.srt);

            /* Get Number of Segments */
            int num_segs = MIN(segment_ph_cnt.gt[PRT_LEFT].size, segment_ph_cnt.gt[PRT_RIGHT].size);
            if(segment_ph_cnt.gt[PRT_LEFT].size != segment_ph_cnt.gt[PRT_RIGHT].size)
            {
                mlog(WARNING, "Segment size mismatch in %s for track %d\n", filename, track);
            }

            /* Initialize Photon Pointers per Pair Reference Track */
            uint32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 };

            /* Go Through Each Segment in File */
            for(int s = 0; s < num_segs; s++)
            {
                /* Get Segment ID */
                uint32_t seg_id = segment_id.gt[PRT_LEFT][s];
                if(segment_id.gt[PRT_LEFT][s] != segment_id.gt[PRT_RIGHT][s])
                {
                    mlog(WARNING, "Segment ID mismatch in %s for segments %d and %d\n", filename, segment_id.gt[PRT_LEFT][s], segment_id.gt[PRT_RIGHT][s]);
                }

                /* Allocate Segment Data */
                int seg_size = sizeof(segment_t) + (sizeof(photon_t) * (segment_ph_cnt.gt[PRT_LEFT][s] + segment_ph_cnt.gt[PRT_RIGHT][s]));
                RecordObject* record = new RecordObject(recType, seg_size); // TODO: overallocated memory... photons filtered below
                segment_t* segment = (segment_t*)record->getRecordData();

                /* Initialize Attributes of Segment */
                segment->track = track;
                segment->segment_id = seg_id;
                segment->num_photons[PRT_LEFT] = 0;
                segment->num_photons[PRT_RIGHT] = 0;

                /* Populate Segment Record Photons */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Loop Through Each Photon in Segment */
                    for(int p = 0; p < segment_ph_cnt.gt[t][s]; p++)
                    {
                        if(signal_conf_ph.gt[t][ph_in[t]] >= parms.cnf)
                        {
                            segment->photons[ph_out].distance_x = segment_dist_x.gt[t][s] + dist_ph_along.gt[t][ph_in[t]];
                            segment->photons[ph_out].height_y = h_ph.gt[t][ph_in[t]];
                            segment->num_photons[t]++;
                            ph_out++;
                        }
                        ph_in[t]++;
                    }
                }

                /* Set Photon Pointer Fields */
                segment->photon_offset[PRT_LEFT] = sizeof(segment_t); // pointers are set to offset from start of record data
                segment->photon_offset[PRT_RIGHT] = sizeof(segment_t) + (sizeof(photon_t) * segment->num_photons[PRT_LEFT]);

                /* Resize Record Data */
                int new_size = sizeof(segment_t) + (sizeof(photon_t) * (segment->num_photons[PRT_LEFT] + segment->num_photons[PRT_RIGHT]));
                record->resizeData(new_size);

                /* Add Segment Record */
                segmentList.add(record);
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
 * luaConfig
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
        lua_getfield(L, 2, "srt");
        lua_obj->parms.srt = (surfaceType_t)getLuaInteger(L, -1, true, lua_obj->parms.srt);

        lua_getfield(L, 2, "cnf");
        lua_obj->parms.cnf = (signalConf_t)getLuaInteger(L, -1, true, lua_obj->parms.cnf);

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
 * luaParms
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Hdf5Atl03Handle* lua_obj = (Hdf5Atl03Handle*)getLuaSelf(L, 1);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "srt", lua_obj->parms.srt);
        LuaEngine::setAttrInt(L, "cnf", lua_obj->parms.cnf);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
