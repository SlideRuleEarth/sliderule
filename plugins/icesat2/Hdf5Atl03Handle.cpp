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
    {NULL,          NULL}
};

const Hdf5Atl03Handle::parms_t Hdf5Atl03Handle::DefaultParms = {
    .srt = SRT_LAND_ICE,
    .conf = CNF_SURFACE_HIGH
};

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<track>, [<id>], [<raw>])
 *----------------------------------------------------------------------------*/
int Hdf5Atl03Handle::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int     track       = getLuaInteger(L, 1);
        long    id          = getLuaInteger(L, 2, true, 0);
        bool    raw_mode    = getLuaBoolean(L, 3, true, true);

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5Atl03Handle(L, track, id, raw_mode));
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
Hdf5Atl03Handle::Hdf5Atl03Handle (lua_State* L, int track, long id, bool raw_mode):
    Hdf5Handle(L, LuaMetaName, LuaMetaTable)
{
    /* Set Track */
    groundTrack = track;

    /* Set Parameters */
    extractParms = DefaultParms;

    /* Set Segment List Index */
    listIndex = 0;

    /* Set Mode */
    rawMode = raw_mode;

    /* Set ID of Dataset Record */
    recData->id = id;
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
    uint8_t track = 1;

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
//            GTArray<char> signal_conf_ph  (file, "heights/signal_conf_ph",        track);

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

                /* Allocate Segment Record */
                size_t seg_size = sizeof(segment_t) + (sizeof(photon_t) * (segment_ph_cnt.gt[PRT_LEFT][s] + segment_ph_cnt.gt[PRT_RIGHT][s]));
                segment_t* seg = (segment_t*) new uint8_t[seg_size];

                /* set Attributes of Segment */
                seg->track = track;
                seg->segment_id = seg_id;
                seg->num_photons[PRT_LEFT] = segment_ph_cnt.gt[PRT_LEFT][s];
                seg->num_photons[PRT_RIGHT] = segment_ph_cnt.gt[PRT_RIGHT][s];

                /* Populate Segment Record Photons */
                uint32_t curr_photon = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    for(unsigned p = 0; p < seg->num_photons[t]; p++)
                    {
                        seg->photons[curr_photon].distance_x = segment_dist_x.gt[t][curr_photon] + dist_ph_along.gt[t][curr_photon];
                        seg->photons[curr_photon].height_y = h_ph.gt[t][curr_photon];
                        curr_photon++;
                    }
                }

                /* Add Segment Record */
                segmentList.add(seg);
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

    /* Read Next Segment in List */
    if(listIndex < segmentList.length())
    {
        int seg_size = sizeof(segment_t) + (sizeof(photon_t) * (segmentList[listIndex]->num_photons[PRT_LEFT] + segmentList[listIndex]->num_photons[PRT_LEFT]));
        segment_t* segment = segmentList[listIndex];

        if(rawMode)
        {
            if(seg_size <= len)
            {
                LocalLib::copy(buf, segment, seg_size);
                bytes_read = seg_size;
            }
            else
            {
                mlog(ERROR, "Unable to read ATL03 segment data, buffer too small (%d > %d)\n", seg_size, len);
            }
        }
        else // record
        {
            if(seg_size <= (len - recObj->getAllocatedMemory()))
            {
                recData->offset = 0;
                recData->size = seg_size;
                unsigned char* rec_buf = (unsigned char*)buf;
                int bytes_written = recObj->serialize(&rec_buf, RecordObject::COPY, len);
                LocalLib::copy(&rec_buf[bytes_written], segment, seg_size);
                bytes_read = bytes_written + seg_size;
            }
            else
            {
                mlog(ERROR, "Unable to read ATL03 segment record, buffer too small (%d > %d - %d)\n", seg_size, len, recObj->getAllocatedMemory());
            }
        }

        /* Release Segment Resources */
        delete [] segmentList[listIndex];

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
