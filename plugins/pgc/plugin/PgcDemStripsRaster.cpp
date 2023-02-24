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
PgcDemStripsRaster::PgcDemStripsRaster(lua_State *L, GeoParms* _parms, const int target_crs, const char* dem_name, const char* geocells):
    VctRaster(L, _parms, target_crs),
    demName(dem_name),
    vsis3Path("/vsis3/pgc-opendata-dems/")
{
    path2geocells = vsis3Path;
    path2geocells.append(dem_name).append("/strips/").append(geocells);
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(std::string& file, double lon, double lat)
{
    /*
     * Strip DEM ﬁles are distributed in folders according to the 1° x 1° geocell in which the geometric center resides.
     * Geocell folder naming refers to the southwest degree corner coordinate
     * (e.g., folder n72e129 will contain all ArcticDEM strip ﬁles with centroids within 72° to 73° north latitude, and 129° to 130° east longitude).
     *
     * https://www.pgc.umn.edu/guides/stereo-derived-elevation-models/pgcs-dem-products-arcticdem-rema-and-earthdem/#section-9
     *
     * NOTE: valid latitude strings are 'n59' and up. Nothing below 59. 'n' is always followed by two digits.
     *       valid longitude strings are 'e/w' followed by zero padded 3 digits.
     *       example:  lat 61, lon -120.3  ->  n61w121
     *                 lat 61, lon  -50.8  ->  n61w051
     *                 lat 61, lon   -5    ->  n61w005
     *                 lat 61, lon    5    ->  n61e005
     */

    /* Round to geocell location */
    int _lon = static_cast<int>(floor(lon));
    int _lat = static_cast<int>(floor(lat));

    char lonBuf[32];
    char latBuf[32];

    sprintf(lonBuf, "%03d", abs(_lon));
    sprintf(latBuf, "%02d", _lat);

    std::string lonStr(lonBuf);
    std::string latStr(latBuf);

    file = path2geocells +
           latStr +
           ((_lon < 0) ? "w" : "e") +
           lonStr +
           ".geojson";

    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getIndexBbox
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexBbox(bbox_t &bbox, double lon, double lat)
{
    /* PgcDEM geocells are 1x1 degree */
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
bool PgcDemStripsRaster::findRasters(OGRPoint& p)
{
    /*
     * Could get date from filename but will read from geojson index file instead.
     * It contains two dates: 'start_date' and 'end_date'
     *
     * The date in the filename is the date of the earliest image of the stereo pair.
     * For intrack pairs (pairs collected intended for stereo) the two images are acquired within a few minutes of each other.
     * For cross-track images (opportunistic stereo pairs made from mono collects)
     * the two images can be from up to 20 days apart.
     *
     */
    const char* demField         = "dem";
    const int DATES_CNT          = 2;
    const char* dates[DATES_CNT] = {"start_datetime", "end_datetime"};
    try
    {
        rastersList->clear();

        /* For now assume the first layer has the feature we need */
        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&p)) continue;

            const char *fname = feature->GetFieldAsString(demField);
            if (fname)
            {
                std::string fileName(fname);
                std::size_t pos = fileName.find(demName);
                if (pos == std::string::npos)
                    throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", demName.c_str());

                fileName = vsis3Path + fileName.substr(pos);

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

                double gps = 0;
                for(int i=0; i<DATES_CNT; i++)
                {
                    TimeLib::gmt_time_t gmtDate;
                    int year, month, day, hour, minute, second, timeZone;
                    bzero(&gmtDate, sizeof(TimeLib::gmt_time_t));
                    year = month = day = hour = minute = second = timeZone = 0;

                    int j = feature->GetFieldIndex(dates[i]);
                    if(feature->GetFieldAsDateTime(j, &year, &month, &day, &hour, &minute, &second, &timeZone))
                    {
                        /* Time Zone flag: 100 is GMT, 1 is localtime, 0 unknown */
                        if(timeZone == 100)
                        {
                            gmtDate.year        = year;
                            gmtDate.doy         = TimeLib::dayofyear(year, month, day);
                            gmtDate.hour        = hour;
                            gmtDate.minute      = minute;
                            gmtDate.second      = second;
                            gmtDate.millisecond = 0;
                        }
                        else mlog(ERROR, "Unsuported time zone in raster date (TMZ is not GMT)");
                    }
                    // mlog(DEBUG, "%04d:%02d:%02d:%02d:%02d:%02d  %s", year, month, day, hour, minute, second, rinfo.fileName.c_str());
                    gps += static_cast<double>(TimeLib::gmt2gpstime(gmtDate));
                }
                gps = gps/DATES_CNT;
                rinfo.gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));
                rinfo.gpsTime = static_cast<int64_t>(gps);
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

    return (rastersList->length() > 0);
}





/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/