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

#include "VrtRaster.h"
#include <vrtdataset.h>
#include <gdal_utils.h>


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void VrtRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void VrtRaster::deinit (void)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
VrtRaster::VrtRaster(lua_State *L, GeoParms* _parms, const char* vrt_file):
    GeoRaster(L, _parms)
{

    useGeoIndex  = false;
    band = NULL;
    bzero(invGeot, sizeof(invGeot));
    groupId = 0;
    if(vrt_file)
        vrtFile = vrt_file;
    else if(_parms->asset)
        vrtFile.append(_parms->asset->getPath()).append("/").append(_parms->asset->getIndex());
}

/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
void VrtRaster::openGeoIndex(double lon, double lat)
{
    std::string newVrtFile;

    getIndexFile(newVrtFile, lon, lat);

    /* Is geoindex already opened with the same file? */
    if (geoIndex.dset != NULL && geoIndex.fileName == newVrtFile)
        return;

    try
    {
        /* Cleanup previous */
        if (geoIndex.dset != NULL)
        {
            GDALClose((GDALDatasetH)geoIndex.dset);
            geoIndex.dset = NULL;
        }

        geoIndex.dset = (GDALDataset *)GDALOpenEx(newVrtFile.c_str(), GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
        if (geoIndex.dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open VRT index file: %s", newVrtFile.c_str());

        mlog(DEBUG, "Opened: %s", newVrtFile.c_str());

        geoIndex.fileName = newVrtFile;
        band = geoIndex.dset->GetRasterBand(1);
        CHECKPTR(band);

        geoIndex.cols = geoIndex.dset->GetRasterXSize();
        geoIndex.rows = geoIndex.dset->GetRasterYSize();

        double geot[6] = {0};
        CPLErr err = geoIndex.dset->GetGeoTransform(geot);
        CHECK_GDALERR(err);

        /* Get raster boundry box */
        geoIndex.bbox.lon_min = geot[0];
        geoIndex.bbox.lon_max = geot[0] + geoIndex.cols * geot[1];
        geoIndex.bbox.lat_max = geot[3];
        geoIndex.bbox.lat_min = geot[3] + geoIndex.rows * geot[5];
        geoIndex.cellSize     = geot[1];

        int radiusInPixels = radius2pixels(geoIndex.cellSize, parms->sampling_radius);

        /* Limit maximum sampling radius */
        if (radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "Sampling radius is too big: %d: max allowed %d meters",
                                   parms->sampling_radius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(geoIndex.cellSize));
        }

        /*
         * Get inverted transform for VRT
         * geot - holds 3x2 coefficients.
         * Inverting switches between geo to pixel <==> pixel to geo
         */
        if (!GDALInvGeoTransform(geot, invGeot))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            CHECK_GDALERR(CE_Failure);
        }

        /* Create coordinates transform for VRT geoIndex */
        createTransform(geoIndex.cord, geoIndex.dset);
    }
    catch (const RunTimeException &e)
    {
        if (geoIndex.dset)
        {
            geoIndex.clear();
            bzero(invGeot, sizeof(invGeot));
            band = NULL;
        }
        throw;
    }
}


/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void VrtRaster::getIndexFile(std::string &file, double lon, double lat)
{
    std::ignore = lon;
    std::ignore = lat;
    file = vrtFile;
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool VrtRaster::findRasters(OGRPoint& p)
{
    std::ignore = p;

    rasterGroupList->clear();

    const char* fname = vrtFile.c_str();
    if(fname)
    {
        rasters_group_t rgroup;
        raster_info_t rinfo;
        rinfo.fileName = fname;
        rinfo.tag      = SAMPLES_RASTER_TAG;

        /* Get the date this raster was created */
        getRasterDate(rinfo);

        /* Create raster group with this raster file in it */
        rgroup.gmtDate = rinfo.gmtDate;
        rgroup.gpsTime = rinfo.gpsTime;
        rgroup.list.add(rgroup.list.length(), rinfo);
        rgroup.id = std::to_string(groupId++);
        rasterGroupList->add(rasterGroupList->length(), rgroup);
    }

    return (rasterGroupList->length() > 0);
}

/*----------------------------------------------------------------------------
 * findCachedRaster
 *----------------------------------------------------------------------------*/
bool VrtRaster::findCachedRasters(OGRPoint& p)
{
    Raster *raster = NULL;

    rasterGroupList->clear();

    const char *key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (containsPoint(raster, p))
        {
            raster->enabled = true;
            raster->point = p;

            /* Store cached raster info, fileName is enough */
            rasters_group_t rgroup;
            raster_info_t rinfo;

            rinfo.fileName = raster->fileName;
            rinfo.tag = SAMPLES_RASTER_TAG;
            rinfo.gpsTime = raster->gpsTime;
            rinfo.gmtDate = TimeLib::gps2gmttime(raster->gpsTime * 1000);
            rgroup.list.add(rgroup.list.length(), rinfo);
            rgroup.gpsTime = rinfo.gpsTime;
            rgroup.gmtDate = rinfo.gmtDate;
            rasterGroupList->add(rasterGroupList->length(), rgroup);

            break; /* Only one raster with this point in VRT */
        }
        key = rasterDict.next(&raster);
    }
    return (rasterGroupList->length() > 0);
}


/*----------------------------------------------------------------------------
 * buildVRT
 *----------------------------------------------------------------------------*/
void VrtRaster::buildVRT(std::string& vrt_file, List<std::string>& rlist)
{
    GDALDataset* vrtDset = NULL;
    std::vector<const char*> rasters;

    for (int i = 0; i < rlist.length(); i++)
    {
        rasters.push_back(rlist[i].c_str());
    }

    vrtDset = (GDALDataset*) GDALBuildVRT(vrt_file.c_str(), rasters.size(), NULL, rasters.data(), NULL, NULL);
    CHECKPTR(vrtDset);
    GDALClose(vrtDset);
    mlog(DEBUG, "Created %s", vrt_file.c_str());
}
