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

/* Landsat 8 */
const char* LandsatHlsRaster::L8_tags[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B09", "B10", "B11",
                                           "SAA", "SZA", "VAA", "VZA", "Fmask"};

/* Sentinel 2 */
const char* LandsatHlsRaster::S2_tags[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B08", "B09", "B10",
                                           "B11", "B12", "B8A", "SAA", "SZA",
                                           "VAA", "VZA", "Fmask"};

#define L8_tagsCnt (sizeof (L8_tags) / sizeof (const char *))
#define S2_tagsCnt (sizeof (S2_tags) / sizeof (const char *))


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
    VctRaster(L, _parms)
{
    filePath.append(_parms->asset->getPath()).append("/");
}


/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexFile(std::string& file, double lon, double lat)
{
#if 1
    file = "/data/hls/hls_trimmed.geojson";
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
    /* L8 tags are a subset of S2 */
    const char** tags = S2_tags;
    const int tagsCnt = S2_tagsCnt;

    try
    {
        rasterGroupList->clear();

        /* For now assume the first layer has the feature we need */
        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&p)) continue;

            rasters_group_t rgroup;
            rgroup.id = feature->GetFieldAsString("id");

            int year, month, day, hour, minute, second, timeZone;
            int i = feature->GetFieldIndex("datetime");
            if(feature->GetFieldAsDateTime(i, &year, &month, &day, &hour, &minute, &second, &timeZone))
            {
                /*
                 * Time Zone flag: 100 is GMT, 1 is localtime, 0 unknown
                 */
                if(timeZone == 100)
                {
                    rgroup.gmtDate.year        = year;
                    rgroup.gmtDate.doy         = TimeLib::dayofyear(year, month, day);
                    rgroup.gmtDate.hour        = hour;
                    rgroup.gmtDate.minute      = minute;
                    rgroup.gmtDate.second      = second;
                    rgroup.gmtDate.millisecond = 0;
                    rgroup.gpsTime             = TimeLib::gmt2gpstime(rgroup.gmtDate);
                }
                else mlog(ERROR, "Unsuported time zone in raster date (TMZ is not GMT)");
            }

            const std::string fileToken = "HLS";

            for(int i=0; i<tagsCnt; i++)
            {
                const char* tag = tags[i];
                const char* fname = feature->GetFieldAsString(tag);
                if(fname && strlen(fname) > 0)
                {
                    std::string fileName(fname);
                    std::size_t pos = fileName.find(fileToken);
                    if(pos == std::string::npos)
                        throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", fileToken.c_str());

                    fileName = filePath + fileName.substr(pos);

                    raster_info_t rinfo;
                    rinfo.fileName = fileName;
                    rinfo.tag = tag;
                    rinfo.gpsTime = rgroup.gpsTime;
                    rinfo.gmtDate = rgroup.gmtDate;

                    rgroup.list.add(rgroup.list.length(), rinfo);
                }
            }
            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup.id.c_str(), rgroup.list.length());
            OGRFeature::DestroyFeature(feature);
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
 * sample
 *----------------------------------------------------------------------------*/
int LandsatHlsRaster::getSamples(double lon, double lat, List<sample_t>& slist, void* param)
{
    std::ignore = param;
    slist.clear();

    try
    {
        /* Get samples, if none found, return */
        if(sample(lon, lat) == 0) return 0;

        Raster* raster = NULL;

        Ordering<rasters_group_t>::Iterator group_iter(*rasterGroupList);
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t& rgroup = group_iter[i].value;
            Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
            uint32_t flags   = 0;

            if(parms->auxiliary_files)
            {
                /* Get flags value for this group of rasters */
                for(int j = 0; j < raster_iter.length; j++)
                {
                    const raster_info_t& rinfo = raster_iter[j].value;
                    if(strcmp("Fmask", rinfo.tag.c_str()) == 0)
                    {
                        const char* key = rinfo.fileName.c_str();
                        if(rasterDict.find(key, &raster))
                        {
                            assert(raster);
                            flags = raster->sample.value;
                        }
                        break;
                    }
                }
            }

            /* Which group is it? Landsat8 or Sentinel2 */
            bool isL8 = false;
            bool isS2 = false;
            std::size_t pos;

            pos = rgroup.id.find("HLS.L30");
            if(pos != std::string::npos) isL8 = true;

            pos = rgroup.id.find("HLS.S30");
            if(pos != std::string::npos) isS2 = true;

            if(!isL8 && !isS2)
                throw RunTimeException(DEBUG, RTE_ERROR, "Could not find valid Landsat8/Sentinel2 groupId");

            double green, red, nir08, swir16;
            green = red = nir08 = swir16 = 0;

            /* Collect samples for all rasters */
            for(int j = 0; j < raster_iter.length; j++)
            {
                const raster_info_t& rinfo = raster_iter[j].value;
                const char* key = rinfo.fileName.c_str();
                if(rasterDict.find(key, &raster))
                {
                    assert(raster);
                    if(raster->enabled && raster->sampled)
                    {
                        /* Update dictionary of used raster files */
                        raster->sample.fileId = fileDictAdd(raster->fileName);
                        raster->sample.flags  = flags;
                        slist.add(raster->sample);

                        /* green and red bands are the same for L8 and S2 */
                        if(rinfo.tag == "B03") green = raster->sample.value;
                        if(rinfo.tag == "B04") red = raster->sample.value;

                        if(isL8)
                        {
                            if(rinfo.tag == "B05") nir08  = raster->sample.value;
                            if(rinfo.tag == "B06") swir16 = raster->sample.value;
                        }
                        else /* Must be Sentinel2 */
                        {
                            if(rinfo.tag == "B8A") nir08  = raster->sample.value;
                            if(rinfo.tag == "B11") swir16 = raster->sample.value;
                        }
                    }
                }
            }

            //TODO: must come from params if calculated and returned
            sample_t sample;
            std::string str = rgroup.id + " {\"algo\": \"";

            if(1)
            {
                double ndsi = (green - swir16) / (green + swir16);
                bzero(&sample, sizeof(sample));
                sample.value  = ndsi;
                sample.fileId = fileDictAdd(str + "NDSI\"}");
                slist.add(sample);
            }

            if(1)
            {
                double ndvi = (nir08 - red) / (nir08 + red);
                bzero(&sample, sizeof(sample));
                sample.value  = ndvi;
                sample.fileId = fileDictAdd(str + "NDVI\"}");
                slist.add(sample);
            }

            if(1)
            {
                double ndwi = (nir08 - swir16) / (nir08 + swir16);
                bzero(&sample, sizeof(sample));
                sample.value  = ndwi;
                sample.fileId = fileDictAdd(str + "NDWI\"}");
                slist.add(sample);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    return slist.length();
}





/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/