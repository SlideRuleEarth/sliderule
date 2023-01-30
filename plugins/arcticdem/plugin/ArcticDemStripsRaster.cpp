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
    /*
     * Strip DEM ﬁles are distributed in folders according to the 1° x 1° geocell in which the geometric center resides.
     * Geocell folder naming refers to the southwest degree corner coordinate
     * (e.g., folder n72e129 will contain all ArcticDEM strip ﬁles with centroids within 72° to 73° north latitude, and 129° to 130° east longitude).
     *
     * https://www.pgc.umn.edu/guides/stereo-derived-elevation-models/pgcs-dem-products-arcticdem-rema-and-earthdem/#section-9
     */

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
bool ArcticDemStripsRaster::findRasters(OGRPoint& p)
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

    bool foundFile = false;

    const std::string fileToken = "arcticdem";
    const std::string vsisPath  = "/vsis3/pgc-opendata-dems/";
    const char *demField  = "dem";
    const char* dateField = "end_datetime";

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
                std::size_t pos = fileName.find(fileToken);
                if (pos == std::string::npos)
                    throw RunTimeException(ERROR, RTE_ERROR, "Could not find marker %s in file", fileToken.c_str());

                fileName = vsisPath + fileName.substr(pos);
                foundFile = true; /* There may be more than one file.. */

                raster_info_t rinfo = {fileName, {0}};

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
                    }
                    else mlog(ERROR, "Unsuported time zone in raster date (TMZ is not GMT)");
                }
                rastersList->add(rinfo);
            }
            OGRFeature::DestroyFeature(feature);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return foundFile;
}





/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/