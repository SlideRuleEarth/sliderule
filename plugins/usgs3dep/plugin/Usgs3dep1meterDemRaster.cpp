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
Usgs3dep1meterDemRaster::Usgs3dep1meterDemRaster(lua_State* L, GeoParms* _parms):
 GeoIndexedRaster(L, _parms, &overrideTargetCRS),
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
Usgs3dep1meterDemRaster::~Usgs3dep1meterDemRaster(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void Usgs3dep1meterDemRaster::getIndexFile(std::string& file, double lon, double lat)
{
    std::ignore = lon;
    std::ignore = lat;
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool Usgs3dep1meterDemRaster::findRasters(GdalRaster::Point& p)
{
    try
    {
        OGRPoint point(p.x, p.y, p.z);

        for(int i = 0; i < featuresList.length(); i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&point)) continue;

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->infovect.reserve(1);
            rgroup->id = feature->GetFieldAsString("id");
            rgroup->gpsTime = getGmtDate(feature, "datetime", rgroup->gmtDate);

            const char* fname = feature->GetFieldAsString("url");
            if(fname && strlen(fname) > 0)
            {
                std::string fileName(fname);
                const size_t pos = strlen(URL_str);

                raster_info_t rinfo;
                rinfo.dataIsElevation = true;
                rinfo.tag             = DEM_TAG;
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


/*----------------------------------------------------------------------------
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
OGRErr Usgs3dep1meterDemRaster::overrideTargetCRS(OGRSpatialReference& target)
{
    OGRErr ogrerr   = OGRERR_FAILURE;
    int northFlag   = 0;
    int utm         = target.GetUTMZone(&northFlag);

    // target.dumpReadable();
    mlog(DEBUG, "Target UTM: %d%s", utm, northFlag?"N":"S");

    /* Must be north */
    if(northFlag==0)
    {
        mlog(ERROR, "Failed to override target CRS, UTM %d%s detected", utm, northFlag?"N":"S");
        return ogrerr;
    }

    const int MIN_UTM = 1;
    const int MAX_UTM = 60;

    if(utm < MIN_UTM || utm > MAX_UTM)
    {
        mlog(ERROR, "Failed to override target CRS, invalid UTM %d%s detected", utm, northFlag ? "N" : "S");
        return ogrerr;
    }

    const int NAVD88_HEIGHT_EPSG          = 5703;
    const int NAD83_2011_UTM_ZONE_1N_EPSG = 6330;

    const int epsg = NAD83_2011_UTM_ZONE_1N_EPSG + utm - 1;
    mlog(DEBUG, "New EPSG: %d", epsg);

    OGRSpatialReference horizontal;
    OGRSpatialReference vertical;

    OGRErr er1 = horizontal.importFromEPSG(epsg);
    OGRErr er2 = vertical.importFromEPSG(NAVD88_HEIGHT_EPSG);
    OGRErr er3 = target.SetCompoundCS("sliderule", &horizontal, &vertical);

    if(((er1 == er2) == er3) == OGRERR_NONE)
    {
        ogrerr = OGRERR_NONE;
    }
    else mlog(ERROR, "Failed to overried target CRS");

    return ogrerr;
    // target.dumpReadable();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/
