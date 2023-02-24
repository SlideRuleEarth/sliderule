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

#include "LandsatHlsRaster.h"
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
LandsatHlsRaster::LandsatHlsRaster(lua_State *L, GeoParms* _parms):
    VctRaster(L, _parms, LANDSAT_HLS_EPSG)
{
}


/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexFile(std::string& file, double lon, double lat)
{
#if 1
    file = "/home/elidwa/ICESat2/sliderule-python/hls_trimmed.geojson";
#else
    /* Round to geocell location */
    int _lon = static_cast<int>(floor(lon));
    int _lat = static_cast<int>(floor(lat));

    char lonBuf[32];
    char latBuf[32];

    sprintf(lonBuf, "%03d", abs(_lon));
    sprintf(latBuf, "%02d", _lat);

    std::string lonStr(lonBuf);
    std::string latStr(latBuf);

    file = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n" +
           latStr +
           ((_lon < 0) ? "w" : "e") +
           lonStr +
           ".geojson";
#endif
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getIndexBbox
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexBbox(bbox_t &bbox, double lon, double lat)
{
    /* ArcticDEM geocells are 1x1 degree */
    const double geocellSize = 1.0;

    lat = floor(lat);
    lon = floor(lon);

    bbox.lon_min = lon;
    bbox.lat_min = lat;
    bbox.lon_max = lon + geocellSize;
    bbox.lat_max = lat + geocellSize;
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::findRasters(OGRPoint& p)
{
#if 1

    const std::string fileToken = "lp-prod-protected";
    const std::string vsisPath  = "/vsis3/lp-prod-protected/";
    const char *demField  = "B04";
    const char* dateField = "datetime";

    try
    {
        rastersList->clear();

        /* For now assume the first layer has the feature we need */
        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            std::string geometryStr = geo->getGeometryName();
            print2term("%s\n", geometryStr.c_str());

            if(!geo->Contains(&p)) continue;

            const char *fname = feature->GetFieldAsString(demField);
            if (fname)
            {
                std::string fileName(fname);
                std::size_t pos = fileName.find(fileToken);
                if (pos == std::string::npos)
                    throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", fileToken.c_str());

                fileName = vsisPath + fileName.substr(pos);

                raster_info_t rinfo;
                rinfo.fileName = fileName;
                bzero(&rinfo.gmtDate, sizeof(TimeLib::gmt_time_t));
                rinfo.gpsTime = 0;

                const std::string endToken    = "_dem.tif";
                const std::string newEndToken = "_bitmask.tif";
                pos = fileName.rfind(endToken);
                if (pos != std::string::npos)
                {
                    fileName.replace(pos, endToken.length(), newEndToken.c_str());
                } else fileName.clear();
                rinfo.auxFileName = fileName;

                int year, month, day, hour, minute, second, timeZone;
                int i = feature->GetFieldIndex(dateField);
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
                        rinfo.gpsTime = TimeLib::gmt2gpstime(rinfo.gmtDate);
                    }
                    else mlog(ERROR, "Unsuported time zone in raster date (TMZ is not GMT)");
                }
                rastersList->add(rastersList->length(), rinfo);
            }
            OGRFeature::DestroyFeature(feature);
        }
        mlog(DEBUG, "Found %ld rasters for (%.2lf, %.2lf)", rastersList->length(), p.getX(), p.getY());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }
#endif
    return (rastersList->length() > 0);
}





/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/