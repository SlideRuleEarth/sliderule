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

#include "Usgs3dep1meterDemRaster.h"
#include "TimeLib.h"

#include <uuid/uuid.h>
#include <limits>

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Usgs3dep1meterDemRaster::URL_str = "https://prd-tnm.s3.amazonaws.com";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Usgs3dep1meterDemRaster::Usgs3dep1meterDemRaster(lua_State *L, GeoParms* _parms):
    VctRaster(L, _parms)
{
    char uuid_str[UUID_STR_LEN];

    if(_parms->catalog == NULL)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty CATALOG/geojson index file received");

    filePath.append(_parms->asset->getPath());
    indexFile = "/vsimem/" + std::string(getUUID(uuid_str)) + ".geojson";

    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(), (GByte*)_parms->catalog, (vsi_l_offset)strlen(_parms->catalog), FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);

    dataIsElevation = true;
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void Usgs3dep1meterDemRaster::getIndexFile(std::string& file, double lon, double lat)
{
    (void)lon;
    (void)lat;
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool Usgs3dep1meterDemRaster::findRasters(OGRPoint& p)
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
                std::string fileName(fname);
                const size_t pos = strlen(URL_str);

                raster_info_t rinfo;
                rinfo.fileName = filePath + fileName.substr(pos);
                rinfo.tag      = SAMPLES_RASTER_TAG;
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
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
void Usgs3dep1meterDemRaster::overrideTargetCRS(OGRSpatialReference& target)
{
    int northFlag   = 0;
    int utm         = target.GetUTMZone(&northFlag);

    // target.dumpReadable();
    mlog(DEBUG, "Target UTM: %d%s", utm, northFlag?"N":"S");

    /* Must be north */
    if(northFlag==0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform, UTM %d%s detected", utm, northFlag?"N":"S");

    const int MIN_UTM = 1;
    const int MAX_UTM = 60;

    if(utm < MIN_UTM || utm > MAX_UTM)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform, invalid UTM %d%s detected", utm, northFlag ? "N" : "S");

    const int NAVD88_HEIGHT_EPSG          = 5703;
    const int NAD83_2011_UTM_ZONE_1N_EPSG = 6330;

    const int epsg = NAD83_2011_UTM_ZONE_1N_EPSG + utm - 1;
    mlog(DEBUG, "New EPSG: %d", epsg);

    OGRSpatialReference horizontal;
    OGRSpatialReference vertical;

    OGRErr ogrerr = horizontal.importFromEPSG(epsg);
    CHECK_GDALERR(ogrerr);
    ogrerr = vertical.importFromEPSG(NAVD88_HEIGHT_EPSG);
    CHECK_GDALERR(ogrerr);

    ogrerr = target.SetCompoundCS("sliderule", &horizontal, &vertical);
    CHECK_GDALERR(ogrerr);
    // target.dumpReadable();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/
