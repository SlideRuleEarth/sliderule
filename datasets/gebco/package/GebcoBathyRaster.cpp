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
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GebcoBathyRaster::GebcoBathyRaster(lua_State* L, RequestFields* rqst_parms, const char* key):
 GeoIndexedRaster(L, rqst_parms, key),
 filePath("/vsis3/" + std::string(parms->asset.asset->getPath())),
 indexFile(parms->asset.asset->getIndex())
{
    /*
     * To support datasets from different years, we use the 'bands' parameter to identify which year's data to sample.
     * This band parameter must be cleared in 'parms' since it does not correspond to an actual band.
     */
    std::string year("2024");
    if(parms->bands.length() == 0)
    {
        mlog(INFO, "Using latest GEBCO data from %s", year.c_str());
    }
    else if(parms->bands.length() == 1)
    {
        if(parms->bands[0] == "2023" || parms->bands[0] == "2024")
        {
            year = parms->bands[0];
            mlog(INFO, "Using GEBCO data from %s", year.c_str());

            /* Clear params->bands */
            const_cast<FieldList<std::string>&>(parms->bands).clear();
        }
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid band name specified");
    }
    else throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid number of bands specified");

    /* Build data path on s3 */
    filePath += "/" + year;
    mlog(DEBUG, "Using data path: %s", filePath.c_str());
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
    mlog(DEBUG, "Using index file: %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void GebcoBathyRaster::getIndexFile(const std::vector<point_info_t>* points, std::string& file)
{
    static_cast<void>(points);
    file = filePath + "/" + indexFile;
    mlog(DEBUG, "Using index file: %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool GebcoBathyRaster::findRasters(raster_finder_t* finder)
{
    const std::vector<OGRFeature*>* flist = finder->featuresList;
    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = 0;
    const uint32_t end     = flist->size();

    try
    {
        for(uint32_t i = start; i < end; i++)
        {
            OGRFeature* feature = flist->at(i);
            OGRGeometry *rastergeo = feature->GetGeometryRef();

            if (!rastergeo->Intersects(geo)) continue;

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->gpsTime = getGmtDate(feature, DATE_TAG, rgroup->gmtDate) / 1000.0;

            const char* dataFile  = feature->GetFieldAsString("data_raster");
            if(dataFile && (strlen(dataFile) > 0))
            {
                raster_info_t rinfo;
                rinfo.elevationBandNum = 1;
                rinfo.tag              = VALUE_TAG;
                rinfo.fileId           = finder->fileDict.add(filePath + "/" + dataFile);
                rgroup->infovect.push_back(rinfo);
            }

            if(parms->flags_file)
            {
                const char* flagsFile = feature->GetFieldAsString("flags_raster");
                if(flagsFile && (strlen(flagsFile) > 0))
                {
                    raster_info_t rinfo;
                    rinfo.flagsBandNum    = 1;
                    rinfo.tag             = FLAGS_TAG;
                    rinfo.fileId          = finder->fileDict.add(filePath + "/" + flagsFile);
                    rgroup->infovect.push_back(rinfo);
                }
            }
            rgroup->infovect.shrink_to_fit();

            mlog(DEBUG, "Added group with %ld rasters", rgroup->infovect.size());
            for(unsigned j = 0; j < rgroup->infovect.size(); j++)
                mlog(DEBUG, "  %s", finder->fileDict.get(rgroup->infovect[j].fileId));

            // Add the group
            finder->rasterGroups.push_back(rgroup);
        }
        finder->rasterGroups.shrink_to_fit();

        mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}
