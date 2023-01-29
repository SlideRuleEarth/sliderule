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

#include "ArcticDemStripsRaster.h"
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
ArcticDemStripsRaster::ArcticDemStripsRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats):
    VctRaster(L, dem_sampling, sampling_radius, zonal_stats, ARCTIC_DEM_EPSG)
{
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoRaster* ArcticDemStripsRaster::create(lua_State* L, const char* dem_sampling, const int sampling_radius, const bool zonal_stats)
{
    return new ArcticDemStripsRaster(L, dem_sampling, sampling_radius, zonal_stats);
}


/*----------------------------------------------------------------------------
 * getRisFile
 *----------------------------------------------------------------------------*/
void ArcticDemStripsRaster::getRisFile(std::string& file, double lon, double lat)
{
    int _lon = static_cast<int>(floor(lon));
    int _lat = static_cast<int>(floor(lat));
    file = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n" +
           std::to_string(_lat) +
           ((_lon < 0) ? "w" : "e") +
           std::to_string(abs(_lon)) +
           ".geojson";

    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
void ArcticDemStripsRaster::getRisBbox(bbox_t &bbox, double lon, double lat)
{
    /* ArcticDEM scenes are 1x1 degree */
    const double sceneSize = 1.0;

    lat = floor(lat);
    lon = floor(lon);

    bbox.lon_min = lon;
    bbox.lat_min = lat;
    bbox.lon_max = lon + sceneSize;
    bbox.lat_max = lat + sceneSize;
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool ArcticDemStripsRaster::findRasters(OGRPoint& p)
{
    bool foundFile = false;

    const std::string fileToken = "arcticdem";
    const std::string vsisPath  = "/vsis3/pgc-opendata-dems/";

    try
    {
        tifList->clear();

        /* For now assume the first layer has the feature we need */
        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&p)) continue;

            int i = feature->GetFieldIndex("dem");
            if (i != -1)
            {
                const char* str = feature->GetFieldAsString("dem");
                if (str)
                {
                    std::string fname(str);
                    std::size_t pos = fname.find(fileToken);
                    if (pos == std::string::npos)
                        throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in file", fileToken.c_str());

                    fname = vsisPath + fname.substr(pos);
                    tifList->add(fname);
                    foundFile = true; /* There may be more than one file.. */
                    break;
                }
#if 0
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
#endif
            }
            OGRFeature::DestroyFeature(feature);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error finding raster in vector index file: %s", e.what());
    }

    return foundFile;
}



/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
int64_t ArcticDemStripsRaster::getRasterDate(std::string &tifFile)
{
    /*
     * For strips the GMT sample collection date is in the tifFile name.
     */

    int64_t gpsTime = 0;

    try
    {
        /* s2s041 is version number for Strips release. This code must be updated when version changes */
        std::string key = "SETSM_s2s041_";

        std::size_t pos = tifFile.rfind(key);
        if (pos == std::string::npos)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in filename", key.c_str());

        std::string id = tifFile.substr(pos + key.length());

        key = "_";
        pos = id.find(key);
        if (pos == std::string::npos)
            throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in filename", key.c_str());

        std::string yearStr  = id.substr(pos + key.length(), 4);
        std::string monthStr = id.substr(pos + key.length() + 4, 2);
        std::string dayStr   = id.substr(pos + key.length() + 4 + 2, 2);

        int year  = strtol(yearStr.c_str(), NULL, 10);
        int month = strtol(monthStr.c_str(), NULL, 10);
        int day   = strtol(dayStr.c_str(), NULL, 10);

        /*
         * Date included in tifFile name for strip rasters is in GMT
         */
        TimeLib::gmt_time_t gmt;
        gmt.year = year;
        gmt.doy = TimeLib::dayofyear(year, month, day);
        gmt.hour = 0;
        gmt.minute = 0;
        gmt.second = 0;
        gmt.millisecond = 0;
        gpsTime = TimeLib::gmt2gpstime(gmt); // returns milliseconds from gps epoch to time specified in gmt_time

        if (gpsTime == 0) throw RunTimeException(ERROR, RTE_ERROR, "Failed to find time");

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from strips tifFile name: %s", e.what());
    }

    return gpsTime;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/