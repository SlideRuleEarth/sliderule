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

#include "GebcoBathyRaster.h"


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
GebcoBathyRaster::GebcoBathyRaster(lua_State* L, GeoParms* _parms):
 GeoIndexedRaster(L, _parms),
 filePath("/vsis3/" + std::string(_parms->asset->getPath())),
 indexFile(_parms->asset->getIndex())
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GebcoBathyRaster::~GebcoBathyRaster(void) = default;

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void GebcoBathyRaster::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    static_cast<void>(geo);
    file = filePath + "/" + indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool GebcoBathyRaster::findRasters(const OGRGeometry* geo)
{
    try
    {
        for(unsigned i = 0; i < featuresList.size(); i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry *rastergeo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if (!rastergeo->Intersects(geo)) continue;

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->infovect.reserve(1);
            rgroup->id = feature->GetFieldAsString("id");
            rgroup->gpsTime = getGmtDate(feature, "datetime", rgroup->gmtDate);

            const char* dataFile  = feature->GetFieldAsString("data_raster");
            if(dataFile && (strlen(dataFile) > 0))
            {
                raster_info_t rinfo;
                rinfo.dataIsElevation = true;
                rinfo.tag             = VALUE_TAG;
                rinfo.fileName        = filePath + "/" + dataFile;
                rgroup->infovect.push_back(rinfo);
            }

            if(parms->flags_file)
            {
                const char* flagsFile = feature->GetFieldAsString("flags_raster");
                if(flagsFile && (strlen(flagsFile) > 0))
                {
                    raster_info_t rinfo;
                    rinfo.dataIsElevation = false;
                    rinfo.tag = FLAGS_TAG;
                    rinfo.fileName = filePath + "/" + flagsFile;
                    rgroup->infovect.push_back(rinfo);
                }
            }

            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup->id.c_str(), rgroup->infovect.size());
            for(unsigned j = 0; j < rgroup->infovect.size(); j++)
                mlog(DEBUG, "  %s", rgroup->infovect[j].fileName.c_str());

            // Add the group to the groupList
            groupList.add(groupList.length(), rgroup);
        }
        mlog(DEBUG, "Found %ld raster groups", groupList.length());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!groupList.empty());
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/
