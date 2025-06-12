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

#include "GeoRaster.h"
#include "GeoIndexedRaster.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double GeoIndexedRaster::TOLERANCE = 0.01;  /* Tolerance for simplification */
const char* GeoIndexedRaster::FLAGS_TAG = "Fmask";
const char* GeoIndexedRaster::VALUE_TAG = "Value";
const char* GeoIndexedRaster::DATE_TAG  = "datetime";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * RasterFinder Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::RasterFinder::RasterFinder (const OGRGeometry* _geo,
                                              const std::vector<OGRFeature*>* _featuresList,
                                              RasterFileDictionary& _fileDict):
    geo(_geo),
    featuresList(_featuresList),
    fileDict(_fileDict)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::GeoIndexedRaster(lua_State *L, RequestFields* rqst_parms, const char* key, GdalRaster::overrideGeoTransform_t gtf_cb, GdalRaster::overrideCRS_t crs_cb):
    RasterObject    (L, rqst_parms, key),
    cache           (MAX_READER_THREADS),
    ssErrors        (SS_NO_ERRORS),
    gtfcb           (gtf_cb),
    crscb           (crs_cb),
    bbox            {0, 0, 0, 0},
    rows            (0),
    cols            (0),
    geoRtree        (parms->sort_by_index)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "dim", luaDimensions);
    LuaEngine::setAttrFunc(L, "bbox", luaBoundingBox);
    LuaEngine::setAttrFunc(L, "cell", luaCellSize);

    /* Establish Credentials */
    GdalRaster::initAwsAccess(parms);
}


/*----------------------------------------------------------------------------
 * getGmtDate
 *----------------------------------------------------------------------------*/
double GeoIndexedRaster::getGmtDate(const OGRFeature* feature, const char* field, TimeLib::gmt_time_t& gmtDate)
{
    const int i = feature->GetFieldIndex(field);
    if(i == -1)
    {
        mlog(ERROR, "Time field: %s not found, unable to get GMT date", field);
        return 0;
    }

    double gpstime = 0;
    double seconds;
    int year;
    int month;
    int day;
    int hour;
    int minute;

    /*
     * Raster's datetime in geojson index file should be properly formated GMT date time string in ISO8601 format.
     * Make best effort to convert it to gps time.
     */
    if(const char* iso8601date = feature->GetFieldAsISO8601DateTime(i, NULL))
    {
        if (sscanf(iso8601date, "%04d-%02d-%02dT%02d:%02d:%lfZ",
            &year, &month, &day, &hour, &minute, &seconds) == 6)
        {
            gmtDate.year = year;
            gmtDate.doy = TimeLib::dayofyear(year, month, day);
            gmtDate.hour = hour;
            gmtDate.minute = minute;
            gmtDate.second = seconds;
            gmtDate.millisecond = 0;
            gpstime = TimeLib::gmt2gpstime(gmtDate);
            // mlog(DEBUG, "%04d:%02d:%02d:%02d:%02d:%02d", year, month, day, hour, minute, (int)seconds);
        }
        else mlog(DEBUG, "Unable to parse ISO8601 UTC date string [%s]", iso8601date);
    }
    else mlog(DEBUG, "Date field is invalid");

    return gpstime;
}

/*----------------------------------------------------------------------------
 * getFeatureDate
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::getFeatureDate(const OGRFeature* feature, TimeLib::gmt_time_t& gmtDate)
{
    if(getGmtDate(feature, DATE_TAG, gmtDate) > 0)
        return true;

    return false;
}

/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::openGeoIndex(const std::string& newFile, OGRGeometry* filter)
{
    /* Trying to open the same file? */
    if(!geoRtree.empty() && newFile == indexFile)
        return true;

    GDALDataset* dset = NULL;
    try
    {
        geoRtree.clear();

        /* Open new vector data set*/
        dset = static_cast<GDALDataset *>(GDALOpenEx(newFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL));
        if (dset == NULL)
        {
            mlog(CRITICAL, "Failed to open vector index file: %s", newFile.c_str());
            throw RunTimeException(ERROR, RTE_FAILURE, "Failed to open vector index file: %s:", newFile.c_str());
        }

        indexFile = newFile;
        OGRLayer* layer = dset->GetLayer(0);
        CHECKPTR(layer);

        if(filter)
        {
            applySpatialFilter(layer, filter);
        }

        /*
         * Insert features into R-tree after applying temporal filter
         */
        layer->ResetReading();
        while(OGRFeature* feature = layer->GetNextFeature())
        {
            /* Temporal filter */
            TimeLib::gmt_time_t gmtDate;
            if(parms->filter_time && getFeatureDate(feature, gmtDate))
            {
                /* Check if feature is in time range */
                if(!TimeLib::gmtinrange(gmtDate, parms->start_time, parms->stop_time))
                {
                    OGRFeature::DestroyFeature(feature);
                    continue;
                }
            }

            /* Insert feature into tree */
            geoRtree.insert(feature);

            /* Destroy feature, R-tree has its own copy */
            OGRFeature::DestroyFeature(feature);
        }

        cols = dset->GetRasterXSize();
        rows = dset->GetRasterYSize();

        /* OGREnvelope is not treated as first classs geometry in OGR, must create a polygon geometry from it */
        OGREnvelope env;
        const OGRErr err = layer->GetExtent(&env);
        if(err == OGRERR_NONE )
        {
            bbox.lon_min = env.MinX;
            bbox.lat_min = env.MinY;
            bbox.lon_max = env.MaxX;
            bbox.lat_max = env.MaxY;
        }

        // mlog(DEBUG, "Loaded %lld features from: %s", layer->GetFeatureCount(), newFile.c_str());
        GDALClose((GDALDatasetH)dset);
    }
    catch (const RunTimeException &e)
    {
        GDALClose((GDALDatasetH)dset);
        geoRtree.clear();
        ssErrors |= SS_INDEX_FILE_ERROR;
        return false;
    }

    return true;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GeoIndexedRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoIndexedRaster *lua_obj = dynamic_cast<GeoIndexedRaster*>(getLuaSelf(L, 1));

        /* Return dimensions of index vector file */
        lua_pushinteger(L, lua_obj->rows);
        lua_pushinteger(L, lua_obj->cols);
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting dimensions: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaBoundingBox - :bbox() --> (lon_min, lat_min, lon_max, lat_max)
 *----------------------------------------------------------------------------*/
int GeoIndexedRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoIndexedRaster *lua_obj = dynamic_cast<GeoIndexedRaster*>(getLuaSelf(L, 1));

        /* Return bbox of index vector file */
        lua_pushnumber(L, lua_obj->bbox.lon_min);
        lua_pushnumber(L, lua_obj->bbox.lat_min);
        lua_pushnumber(L, lua_obj->bbox.lon_max);
        lua_pushnumber(L, lua_obj->bbox.lat_max);
        num_ret += 4;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting bounding box: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaCellSize - :cell() --> cell size
 *----------------------------------------------------------------------------*/
int GeoIndexedRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Cannot return cell sizes of index vector file */
        const int cellSize = 0;

        /* Set Return Values */
        lua_pushnumber(L, cellSize);
        num_ret += 1;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting cell size: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}


/*----------------------------------------------------------------------------
 * filterRasters
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::filterRasters(int64_t gps_secs, GroupOrdering* groupList, RasterFileDictionary& dict)
{
    /* NOTE: temporal filter is applied in openGeoIndex() */
    if(!parms->url_substring.value.empty() || parms->filter_doy_range)
    {
        const GroupOrdering::Iterator group_iter(*groupList);
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t* rgroup = group_iter[i].value;
            bool removeGroup = false;

            for(const auto& rinfo : rgroup->infovect)
            {
                /* URL filter */
                if(!parms->url_substring.value.empty())
                {
                    const std::string fileName = dict.get(rinfo.fileId);
                    if(fileName.find(parms->url_substring.value) == std::string::npos)
                    {
                        removeGroup = true;
                        break;
                    }
                }

                /* Day Of Year filter */
                if(parms->filter_doy_range)
                {
                    const bool inrange = TimeLib::doyinrange(rgroup->gmtDate, parms->doy_start, parms->doy_end);
                    if(parms->doy_keep_inrange)
                    {
                        if(!inrange)
                        {
                            removeGroup = true;
                            break;
                        }
                    }
                    else /* Filter out rasters in doy range */
                    {
                        if(inrange)
                        {
                            removeGroup = true;
                            break;
                        }
                    }
                }
            }

            if(removeGroup)
            {
                groupList->remove(group_iter[i].key);
            }
        }
    }

    /* Closest time filter - using raster group time, not individual reaster time */
    int64_t closestGps = 0;
    if(gps_secs > 0)
    {
        /* Caller provided gps time, use it insead of time from params */
        closestGps = gps_secs;
    }
    else if(parms->filter_closest_time)
    {
        /* Params provided closest time */
        closestGps = TimeLib::gmt2gpstime(parms->closest_time) / 1000;
    }

    if(closestGps > 0)
    {
        int64_t minDelta = abs(std::numeric_limits<int64_t>::max() - closestGps);

        /* Find raster group with the closest time */
        const GroupOrdering::Iterator group_iter(*groupList);
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t* rgroup = group_iter[i].value;
            const int64_t gpsTime = rgroup->gpsTime;
            const int64_t delta   = abs(closestGps - gpsTime);

            if(delta < minDelta)
                minDelta = delta;
        }

        /* Remove all groups with greater time delta */
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t* rgroup = group_iter[i].value;
            const int64_t gpsTime = rgroup->gpsTime;
            const int64_t delta   = abs(closestGps - gpsTime);

            if(delta > minDelta)
            {
                groupList->remove(group_iter[i].key);
            }
        }
    }

    return (!groupList->empty());
}

/*----------------------------------------------------------------------------
 * applySpatialFilter
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::applySpatialFilter(OGRLayer* layer, OGRGeometry* filter)
{
    mlog(INFO, "Features before spatial filter: %lld", layer->GetFeatureCount());

    const double startTime = TimeLib::latchtime();

    /* Buffered points generates more detailed filter polygon but is much slower than
     * convex hull, especially for large number of points.
     */
    layer->SetSpatialFilter(filter);
    perfStats.spatialFilterTime = TimeLib::latchtime() - startTime;
    mlog(INFO, "Features after spatial filter: %lld", layer->GetFeatureCount());
    mlog(DEBUG, "Spatial filter time: %.3lf", perfStats.spatialFilterTime);
}