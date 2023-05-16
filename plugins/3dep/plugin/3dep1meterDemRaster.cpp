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

#include "3dep1meterDemRaster.h"
#include "TimeLib.h"

#include <uuid/uuid.h>
#include <limits>

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
threeDep1meterDemRaster::threeDep1meterDemRaster(lua_State *L, GeoParms* _parms):
    VctRaster(L, _parms)
{
    char uuid_str[UUID_STR_LEN];

    if(_parms->catalog == NULL)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty CATALOG/geojson index file received");

    filePath.append(_parms->asset->getPath()).append("/");
    indexFile = "/vsimem/" + std::string(getUUID(uuid_str)) + ".geojson";

    // print2term("%s\n", _parms->catalog);
    // mlog(DEBUG, "%s", _parms->catalog);


    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(), (GByte*)_parms->catalog, (vsi_l_offset)strlen(_parms->catalog), FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void threeDep1meterDemRaster::getIndexFile(std::string& file, double lon, double lat)
{
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getIndexBbox
 *----------------------------------------------------------------------------*/
void threeDep1meterDemRaster::getIndexBbox(bbox_t &bbox, double lon, double lat)
{
    OGREnvelope env;
    OGRErr err = layer->GetExtent(&env);

    bbox.lon_min = env.MinX;
    bbox.lat_min = env.MinY;
    bbox.lon_max = env.MaxX;
    bbox.lat_max = env.MaxY;

    mlog(DEBUG, "Layer extent/bbox: (%.6lf, %.6lf), (%.6lf, %.6lf)", bbox.lon_min, bbox.lat_min, bbox.lon_max, bbox.lat_max);
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool threeDep1meterDemRaster::findRasters(OGRPoint& p)
{
    try
    {
        rasterGroupList->clear();

        for(int i = 0; i < featuresList.length(); i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&p)) continue;

            rasters_group_t rgroup;
            rgroup.id = feature->GetFieldAsString("id");
            double gps = getGmtDate(feature, "datetime", rgroup.gmtDate);
            rgroup.gpsTime = gps;

            const char* fname = feature->GetFieldAsString("url");
            if(fname && strlen(fname) > 0)
            {
                raster_info_t rinfo;
                rinfo.fileName = fname;
                rinfo.tag      = "url";
                rinfo.gpsTime  = rgroup.gpsTime;
                rinfo.gmtDate  = rgroup.gmtDate;

                rgroup.list.add(rgroup.list.length(), rinfo);
            }

            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup.id.c_str(), rgroup.list.length());
            rasterGroupList->add(rasterGroupList->length(), rgroup);
        }
        mlog(DEBUG, "Found %ld raster groups for (%.2lf, %.2lf)", rasterGroupList->length(), p.getX(), p.getY());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (rasterGroupList->length() > 0);
}


/*----------------------------------------------------------------------------
 * getGroupSamples
 *----------------------------------------------------------------------------*/
void threeDep1meterDemRaster::getGroupSamples (const rasters_group_t& rgroup, List<sample_t>& slist, uint32_t flags)
{
    /* Collect samples for all rasters */
    Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
    for(int j = 0; j < raster_iter.length; j++)
    {
        const raster_info_t& rinfo = raster_iter[j].value;
        const char* key            = rinfo.fileName.c_str();
        Raster* raster             = NULL;
        if(rasterDict.find(key, &raster))
        {
            assert(raster);
            if(raster->enabled && raster->sampled)
            {
                /* Update dictionary of used raster files */
                raster->sample.fileId = fileDictAdd(raster->fileName);
                raster->sample.flags  = flags;
                slist.add(raster->sample);
            }
        }
    }
}





/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/
