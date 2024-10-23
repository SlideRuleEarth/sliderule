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
Usgs3dep1meterDemRaster::Usgs3dep1meterDemRaster(lua_State* L, RequestFields* rqst_parms, const char* key):
 GeoIndexedRaster(L, rqst_parms, key, &overrideTargetCRS),
 filePath(parms->asset.asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    /* Create in memory index file */
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
Usgs3dep1meterDemRaster::~Usgs3dep1meterDemRaster(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void Usgs3dep1meterDemRaster::getIndexFile(const OGRGeometry* geo, std::string& file, const std::vector<point_info_t>* points)
{
    static_cast<void>(geo);
    static_cast<void>(points);
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool Usgs3dep1meterDemRaster::findRasters(raster_finder_t* finder)
{
    const std::vector<OGRFeature*>* flist = finder->featuresList;
    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = 0;
    const uint32_t end     = flist->size();

    try
    {
        /* Linearly search through feature list */
        for(uint32_t i = start; i < end; i++)
        {
            OGRFeature* feature = flist->at(i);
            OGRGeometry *rastergeo = feature->GetGeometryRef();

            if (!rastergeo->Intersects(geo)) continue;

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->featureId = feature->GetFieldAsString("id");
            rgroup->gpsTime = getGmtDate(feature, DATE_TAG, rgroup->gmtDate);

            const char* fname = feature->GetFieldAsString("url");
            if(fname && strlen(fname) > 0)
            {
                const std::string fileName(fname);
                const size_t pos = strlen(URL_str);

                raster_info_t rinfo;
                rinfo.dataIsElevation = true;
                rinfo.tag             = VALUE_TAG;
                rinfo.fileName        = filePath + fileName.substr(pos);
                rgroup->infovect.push_back(rinfo);
            }

            // mlog(DEBUG, "Added group: %s with %ld rasters", rgroup->featureId.c_str(), rgroup->infovect.size());
            finder->rasterGroups.push_back(rgroup);
        }
        // mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/*----------------------------------------------------------------------------
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
OGRErr Usgs3dep1meterDemRaster::overrideTargetCRS(OGRSpatialReference& target)
{
    OGRErr ogrerr   = OGRERR_FAILURE;
    int northFlag   = 0;
    const int utm   = target.GetUTMZone(&northFlag);

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

    const OGRErr er1 = horizontal.importFromEPSG(epsg);
    const OGRErr er2 = vertical.importFromEPSG(NAVD88_HEIGHT_EPSG);
    const OGRErr er3 = target.SetCompoundCS("sliderule", &horizontal, &vertical);

    if(er1 == OGRERR_NONE && er2 == OGRERR_NONE && er3 == OGRERR_NONE)
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
