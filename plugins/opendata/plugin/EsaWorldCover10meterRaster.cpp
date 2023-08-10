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

#include "EsaWorldCover10meterRaster.h"


/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/* NOTE:
 * The ESA World Cover AWS url is at: https://esa-worldcover.s3.amazonaws.com
 * but the stack end point does not use it in its query responses.
 */
const char* EsaWorldCover10meterRaster::URL_str = "s3://";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EsaWorldCover10meterRaster::EsaWorldCover10meterRaster(lua_State* L, GeoParms* _parms):
 GeoIndexedRaster(L, _parms),
 filePath(_parms->asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    if(_parms->catalog == NULL)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty CATALOG/geojson index file received");

    /* Create in memory index file */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(), (GByte*)_parms->catalog, (vsi_l_offset)strlen(_parms->catalog), FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EsaWorldCover10meterRaster::~EsaWorldCover10meterRaster(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void EsaWorldCover10meterRaster::getIndexFile(std::string& file, double lon, double lat)
{
    std::ignore = lon;
    std::ignore = lat;
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool EsaWorldCover10meterRaster::findRasters(GdalRaster::Point& p)
{
    std::vector<const char*> dates = {"start_datetime", "end_datetime"};
    try
    {
        OGRPoint point(p.x, p.y, p.z);

        for(int i = 0; i < featuresList.length(); i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&point)) continue;

            double gps = 0;
            for(auto& s : dates)
            {
                TimeLib::gmt_time_t gmt;
                gps += getGmtDate(feature, s, gmt);
            }
            gps = gps / dates.size();

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->infovect.reserve(1);
            rgroup->id = feature->GetFieldAsString("id");
            rgroup->gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));
            rgroup->gpsTime = static_cast<int64_t>(gps);

            const char* fname = feature->GetFieldAsString("url");
            if(fname && strlen(fname) > 0)
            {
                std::string fileName(fname);
                const size_t pos = strlen(URL_str);

                raster_info_t rinfo;
                rinfo.dataIsElevation = false;
                rinfo.tag             = VALUE_TAG;
                rinfo.fileName        = filePath + fileName.substr(pos);
                rgroup->infovect.push_back(rinfo);
            }

            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup->id.c_str(), rgroup->infovect.size());
            groupList.add(groupList.length(), rgroup);
        }
        mlog(DEBUG, "Found %ld raster groups for (%.2lf, %.2lf)", groupList.length(), point.getX(), point.getY());
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
