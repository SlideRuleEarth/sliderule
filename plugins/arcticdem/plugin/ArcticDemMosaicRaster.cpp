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

#include "ArcticDemMosaicRaster.h"
#include "TimeLib.h"

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
ArcticDemMosaicRaster::ArcticDemMosaicRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats):
    VrtRaster(L, dem_sampling, sampling_radius, zonal_stats)
{
    /*
     * ArcticDemMosaicRaster uses one large mosaic VRT file (raster index file)
     * Open it.
     */
    if (!openRasterIndexSet())
        throw RunTimeException(CRITICAL, RTE_ERROR, "Constructor %s failed", __FUNCTION__);

    /*
     * For mosaic, there is only one raster with point of interest in it.
     * Look for it in cache first, it may already be opened.
     */
    setCheckCacheFirst(true);

    /*
     * Only one thread is used to read mosaic tiles.
     * Allow this thread to read directly from raster index data set (VRT) if needed.
     */
    setAllowIndexDataSetSampling(true);
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoRaster* ArcticDemMosaicRaster::create(lua_State* L, const char* dem_sampling, const int sampling_radius, const bool zonal_stats)
{
    return new ArcticDemMosaicRaster(L, dem_sampling, sampling_radius, zonal_stats);
}


/*----------------------------------------------------------------------------
 * getRasterIndexFileName
 *----------------------------------------------------------------------------*/
void ArcticDemMosaicRaster::getRasterIndexFileName(std::string& file, double lon, double lat)
{
    file = "/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/2m_dem_tiles.vrt";
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
int64_t ArcticDemMosaicRaster::getRasterDate(std::string &tifFile)
{
    /*
     * For mosaics the GMT sample collection date is not in the tifFile name.
     * There is a .json file in s3 bucket where tif file is located which contais date information.
     */

    std::string featureFile = tifFile;
    int64_t gpsTime = 0;

    GDALDataset *dset = NULL;

    const std::string key       = "_reg_dem.tif";
    const std::string fieldName = "end_datetime";
    const std::string fileType  = ".json";

    try
    {
        std::size_t pos = featureFile.rfind(key);
        if (pos == std::string::npos)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in file", key.c_str());

        featureFile.replace(pos, key.length(), fileType.c_str());

        dset = (GDALDataset *)GDALOpenEx(featureFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        if (dset == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not open %s file", featureFile.c_str());

        /* For now assume the first layer has the feature we need */
        OGRLayer *layer = dset->GetLayer(0);
        if (layer == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "No layers found in feature file: %s", featureFile.c_str());

        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            int i = feature->GetFieldIndex(fieldName.c_str());
            if (i != -1)
            {
                int year, month, day, hour, minute, second, timeZone;
                if (feature->GetFieldAsDateTime(i, &year, &month, &day, &hour, &minute, &second, &timeZone))
                {
                    /*
                     * Time Zone flag: 100 is GMT, 1 is localtime, 0 unknown
                     */
                    if (timeZone == 100)
                    {
                        TimeLib::gmt_time_t gmt;
                        gmt.year = year;
                        gmt.doy = TimeLib::dayofyear(year, month, day);
                        gmt.hour = hour;
                        gmt.minute = minute;
                        gmt.second = second;
                        gmt.millisecond = 0;
                        gpsTime = TimeLib::gmt2gpstime(gmt); // returns milliseconds from gps epoch to time specified in gmt_time
                    }
                }
                OGRFeature::DestroyFeature(feature);
                break;
            }
            OGRFeature::DestroyFeature(feature);
        }

        if (gpsTime == 0) throw RunTimeException(ERROR, RTE_ERROR, "Failed to find time");

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    if (dset) GDALClose((GDALDatasetH)dset);

    return gpsTime;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/



