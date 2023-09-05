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

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::PgcDemStripsRaster(lua_State *L, GeoParms* _parms, const char* dem_name, const char* geo_suffix, GdalRaster::overrideCRS_t cb):
    GeoIndexedRaster(L, _parms, cb),
    demName(dem_name),
    path2geocells(_parms->asset->getPath() + std::string(geo_suffix)),
    groupId(0)
{

    std::size_t pos = path2geocells.find(demName);
    if (pos == std::string::npos)
        throw RunTimeException(DEBUG, RTE_ERROR, "Invalid path to geocells: %s", path2geocells.c_str());

    filePath = path2geocells.substr(0, pos);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::~PgcDemStripsRaster(void)
{
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    if(GdalRaster::ispoint(geo))
    {
        /*
     * Strip DEM ﬁles are distributed in folders according to the 1° x 1° geocell in which the geometric center resides.
     * Geocell folder naming refers to the southwest degree corner coordinate
     * (e.g., folder n72e129 will contain all ArcticDEM strip ﬁles with centroids within 72° to 73° north latitude, and 129° to 130° east longitude).
     *
     * https://www.pgc.umn.edu/guides/stereo-derived-elevation-models/pgcs-dem-products-arcticdem-rema-and-earthdem/#section-9
     *
     * NOTE: valid latitude strings for Arctic DEMs are 'n59' and up. Nothing below 59. 'n' is always followed by two digits.
     *       valid latitude strings for REMA are 's54' and down. Nothing above 54. 's' is always followed by two digits.
     *       valid longitude strings are 'e/w' followed by zero padded 3 digits.
     *       example:  lat 61, lon -120.3  ->  n61w121
     *                 lat 61, lon  -50.8  ->  n61w051
     *                 lat 61, lon   -5    ->  n61w005
     *                 lat 61, lon    5    ->  n61e005
     */

        const OGRPoint* poi = geo->toPoint();

        /* Round to geocell location */
        int lon = static_cast<int>(floor(poi->getX()));
        int lat = static_cast<int>(floor(poi->getY()));

        char lonBuf[32];
        char latBuf[32];

        sprintf(lonBuf, "%03d", abs(lon));
        sprintf(latBuf, "%02d", abs(lat));

        std::string lonStr(lonBuf);
        std::string latStr(latBuf);

        file = path2geocells +
               latStr +
               ((lon < 0) ? "w" : "e") +
               lonStr +
               ".geojson";

        mlog(DEBUG, "Using %s", file.c_str());
    }
    else if(GdalRaster::ispoly(geo))
    {
        const OGRPolygon* poly = geo->toPolygon();
        //TODO
    }
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::findRasters(const OGRGeometry* geo)
{
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
    std::vector<const char*> dates = {"start_datetime", "end_datetime"};
    try
    {
        for(int i = 0; i < featuresList.length(); i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry* rgeo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(GdalRaster::ispoint(geo))
            {
                if(!rgeo->Contains(geo)) continue;
            }
            else if(GdalRaster::ispoly(geo))
            {
                if(!geo->Intersects(rgeo)) continue;
            }
            else return false;

            /* geojson index files hosted by PGC only contain listing of dems
             * In order to read quality mask raster for each strip we need to build a path to it.
             */
            const char *fname = feature->GetFieldAsString("Dem");
            if(fname && strlen(fname) > 0)
            {
                std::string fileName(fname);
                std::size_t pos = fileName.find(demName);
                if (pos == std::string::npos)
                    throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", demName.c_str());

                fileName = filePath + fileName.substr(pos);

                rasters_group_t* rgroup = new rasters_group_t;
                rgroup->infovect.reserve(2); /* PGC group is 1 data raster + flags raster */

                raster_info_t demRinfo;
                demRinfo.dataIsElevation = true;
                demRinfo.tag = VALUE_TAG;
                demRinfo.fileName = fileName;

                /* bitmask rasters only make sense for poi, don't read them for poly cases */
                if(GdalRaster::ispoint(geo))
                {
                    const std::string endToken    = "_dem.tif";
                    const std::string newEndToken = "_bitmask.tif";
                    pos = fileName.rfind(endToken);
                    if(pos != std::string::npos)
                    {
                        fileName.replace(pos, endToken.length(), newEndToken.c_str());
                    }
                    else fileName.clear();

                    raster_info_t flagsRinfo;
                    flagsRinfo.tag = FLAGS_TAG;
                    flagsRinfo.fileName = fileName;

                    if(flagsRinfo.fileName.length() > 0)
                    {
                        rgroup->infovect.push_back(flagsRinfo);
                    }
                }

                double gps = 0;
                for(auto& s: dates)
                {
                    TimeLib::gmt_time_t gmt;
                    gps += getGmtDate(feature, s, gmt);
                }
                gps = gps/dates.size();

                /* Set raster group time and group id */
                rgroup->gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));
                rgroup->gpsTime = static_cast<int64_t>(gps);
                rgroup->infovect.push_back(demRinfo);
                groupList.add(groupList.length(), rgroup);
            }
        }
        mlog(DEBUG, "Found %ld raster groups", groupList.length());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (groupList.length() > 0);
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/