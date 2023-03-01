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

#include "PgcDemMosaicRaster.h"
#include "TimeLib.h"

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PgcDemMosaicRaster::PgcDemMosaicRaster(lua_State *L, GeoParms* _parms):
    VrtRaster(L, _parms)
{
    /*
     * PgcDemMosaicRaster uses one mosaics VRT file;
     */
    openGeoIndex();
}


/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
bool PgcDemMosaicRaster::mosaicGetRasterDate(raster_info_t& rinfo, const char* token)
{
    /*
     * There is a metadata .json file in s3 bucket where raster is located.
     * It contains two dates: 'start_date' and 'end_date'
     *
     * There isn't really a concept of a single date that applies to the mosaic tiles.
     * The raster creation date is just the processing date and doesn't have anything to do with the date of the source pixels.
     */

    std::string featureFile = rinfo.fileName;
    bool foundDate = false;

    GDALDataset *dset = NULL;

    const std::string key       = token;
    const std::string fileType  = ".json";
    const char* dateField       = "end_datetime";

    bzero(&rinfo.gmtDate, sizeof(TimeLib::gmt_time_t));

    try
    {
        std::size_t pos = featureFile.rfind(key);
        if (pos == std::string::npos)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in file", key.c_str());

        featureFile.replace(pos, key.length(), fileType.c_str());

        dset = (GDALDataset *)GDALOpenEx(featureFile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
        if (dset == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not open %s file", featureFile.c_str());

        OGRLayer *layer = dset->GetLayer(0);
        if (layer == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "No layers found in feature file: %s", featureFile.c_str());

        layer->ResetReading();
        if(OGRFeature* feature = layer->GetNextFeature())
        {
            int i = feature->GetFieldIndex(dateField);
            if (i != -1)
            {
                int year, month, day, hour, minute, second, timeZone;
                year = month = day = hour = minute = second = timeZone = 0;
                if (feature->GetFieldAsDateTime(i, &year, &month, &day, &hour, &minute, &second, &timeZone))
                {
                    /*
                     * Time Zone flag: 100 is GMT, 1 is localtime, 0 unknown
                     */
                    if (timeZone == 100)
                    {
                        rinfo.gmtDate.year = year;
                        rinfo.gmtDate.doy = TimeLib::dayofyear(year, month, day);
                        rinfo.gmtDate.hour = hour;
                        rinfo.gmtDate.minute = minute;
                        rinfo.gmtDate.second = second;
                        rinfo.gmtDate.millisecond = 0;
                        foundDate = true;
                    }
                    else mlog(ERROR, "Unsuported time zone in raster date (TMZ is not GMT)");
                }
            }
            OGRFeature::DestroyFeature(feature);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    if (dset) GDALClose((GDALDatasetH)dset);

    return foundDate;
}

