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

#include "PgcDemStripsRaster.h"
#include <algorithm>


/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

static const std::vector<const char*> dates = {"start_datetime", "end_datetime"};

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::PgcDemStripsRaster(lua_State *L, RequestFields* rqst_parms, const char* key, const char* dem_name, GdalRaster::overrideCRS_t cb):
    GeoIndexedRaster(L, rqst_parms, key, NULL, cb),
    filePath(parms->asset.asset->getPath()),
    indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson"),
    demName(dem_name)
{
    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(),
                                        const_cast<GByte*>(reinterpret_cast<const GByte*>(parms->catalog.value.c_str())), // source bytes
                                        static_cast<vsi_l_offset>(parms->catalog.value.size()), // length in bytes
                                        FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::~PgcDemStripsRaster(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getFeatureDate
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::getFeatureDate(const OGRFeature* feature, TimeLib::gmt_time_t& gmtDate)
{
    double gps = 0;

    for(const auto& s : dates)
    {
        TimeLib::gmt_time_t gmt;
        gps += getGmtDate(feature, s, gmt);
    }
    gps = gps / dates.size();
    gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));

    return true;
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    static_cast<void>(geo);
    file = indexFile;
    // mlog(DEBUG, "Using %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(const std::vector<point_info_t>* points, std::string& file)
{
    static_cast<void>(points);
    file = indexFile;
    // mlog(DEBUG, "Using %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::findRasters(raster_finder_t* finder)
{
    const std::vector<OGRFeature*>* flist = finder->featuresList;
    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = 0;
    const uint32_t end     = flist->size();

    /*
     * Find rasters and their dates.
     * geojson index file contains two dates: 'start_date' and 'end_date'
     * Calculate the rster date as mid point between start and end dates.
     *
     * The file name/path contains a date in it.
     * We cannot use it because it is the date of the earliest image of the stereo pair.
     * For intrack pairs (pairs collected intended for stereo) the two images are acquired within a few minutes of each other.
     * For cross-track images (opportunistic stereo pairs made from mono collects)
     * the two images can be up to 30 days apart.
     *
     */
    try
    {
        for(uint32_t i = start; i < end; i++)
        {
            OGRFeature* feature = flist->at(i);
            OGRGeometry* rastergeo = feature->GetGeometryRef();

            if (!rastergeo->Intersects(geo)) continue;

            const char *fname = feature->GetFieldAsString("dem");
            if(fname && strlen(fname) > 0)
            {
                std::string fileName(fname);
                std::size_t pos = fileName.find(demName);
                if (pos == std::string::npos)
                    throw RunTimeException(DEBUG, RTE_FAILURE, "Could not find marker %s in file", demName.c_str());

                fileName = filePath + fileName.substr(pos);

                rasters_group_t* rgroup = new rasters_group_t;
                raster_info_t demRinfo;
                demRinfo.elevationBandNum = 1;
                demRinfo.tag = VALUE_TAG;
                demRinfo.fileId = finder->fileDict.add(fileName);

                /* bitmask raster, ie flags_file */
                if(parms->flags_file)
                {
                    const std::string endToken    = "_dem.tif";
                    const std::string newEndToken = "_bitmask.tif";
                    pos = fileName.rfind(endToken);
                    if(pos != std::string::npos)
                    {
                        fileName.replace(pos, endToken.length(), newEndToken.c_str());
                    }
                    else fileName.clear();

                    if(!fileName.empty())
                    {
                        raster_info_t flagsRinfo;
                        flagsRinfo.flagsBandNum = 1;
                        flagsRinfo.tag = FLAGS_TAG;
                        flagsRinfo.fileId = finder->fileDict.add(fileName);
                        rgroup->infovect.push_back(flagsRinfo);
                    }
                }

                double gps_msecs = 0;
                for(const auto& s: dates)
                {
                    TimeLib::gmt_time_t gmt;
                    gps_msecs += getGmtDate(feature, s, gmt);
                }
                gps_msecs = gps_msecs/dates.size();

                /* Set raster group time and group id */
                rgroup->gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps_msecs));
                rgroup->gpsTime = static_cast<int64_t>(gps_msecs / 1000.0);
                rgroup->infovect.push_back(demRinfo);
                rgroup->infovect.shrink_to_fit();
                finder->rasterGroups.push_back(rgroup);
            }
        }
        // mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/