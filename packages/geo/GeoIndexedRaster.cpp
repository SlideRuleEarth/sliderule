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

#include "GeoIndexedRaster.h"

#include <algorithm>
#include <gdal.h>
#include <gdalwarper.h>
#include <gdal_priv.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoIndexedRaster::FLAGS_TAG = "Fmask";
const char* GeoIndexedRaster::DEM_TAG   = "Dem";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::deinit (void)
{
}


/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getSamples(double lon, double lat, double height, int64_t gps, List<RasterSample>& slist, void* param)
{
    std::ignore = param;

    samplingMutex.lock();
    try
    {
        /* Get samples, if none found, return */
        if(sample(lon, lat, height, gps) == 0)
        {
            samplingMutex.unlock();
            return;
        }

        Ordering<rasters_group_t>::Iterator iter(*groupList);
        for(int i = 0; i < iter.length; i++)
        {
            const rasters_group_t& rgroup = iter[i].value;
            uint32_t flags = 0;

            /* Get flags value for this group of rasters */
            if(parms->flags_file)
                flags = getGroupFlags(rgroup);

            getGroupSamples(rgroup, slist, flags);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
        samplingMutex.unlock();
        throw;  // rethrow exception
    }
    samplingMutex.unlock();
}


/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::~GeoIndexedRaster(void)
{
    /* Terminate all reader threads */
    for(auto& reader : readers)
    {
        if(reader->thread != NULL)
        {
            reader->sync->lock();
            {
                reader->raster = NULL;  /* No raster to read     */
                reader->run    = false; /* Set run flag to false */
                reader->sync->signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
            }
            reader->sync->unlock();

            delete reader->thread; /* delete thread waits on thread to join */
            delete reader->sync;
        }
    }

    /* Close all rasters */
    cacheitem_t* item;
    const char* key  = cache.first(&item);
    while (key != NULL)
    {
        cache.remove(key);
        delete item->raster;
        delete item;
        key = cache.next(&item);
    }

    if (groupList) delete groupList;

    destroyFeaturesList();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::GeoIndexedRaster(lua_State *L, GeoParms* _parms, GdalRaster::overrideCRS_t cb):
    RasterObject (L, _parms),
    groupList    (new Ordering<rasters_group_t>),
    crscb        (cb)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "dim", luaDimensions);
    LuaEngine::setAttrFunc(L, "bbox", luaBoundingBox);
    LuaEngine::setAttrFunc(L, "cell", luaCellSize);

    /* Establish Credentials */
    GdalRaster::initAwsAccess(_parms);
}

/*----------------------------------------------------------------------------
 * getGroupSamples
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getGroupSamples (const rasters_group_t& rgroup, List<RasterSample>& slist, uint32_t flags)
{
    Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
    for(int i = 0; i < raster_iter.length; i++)
    {
        const raster_info_t& rinfo = raster_iter[i].value;
        if(strcmp(DEM_TAG, rinfo.tag.c_str()) == 0)
        {
            const char* key = rinfo.fileName.c_str();
            cacheitem_t* item;
            if(cache.find(key, &item))
            {
                if(item->enabled && item->raster->sampled())
                {
                    /* Update dictionary of used raster files */
                    RasterSample& sample = item->raster->getSample();
                    sample.fileId = fileDictAdd(item->raster->getFileName());
                    sample.flags  = flags;
                    slist.add(sample);
                }
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * getGroupFlags
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getGroupFlags(const rasters_group_t& rgroup)
{
    uint32_t flags = 0;

    Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
    for(int j = 0; j < raster_iter.length; j++)
    {
        const raster_info_t& rinfo = raster_iter[j].value;
        if(strcmp(FLAGS_TAG, rinfo.tag.c_str()) == 0)
        {
            cacheitem_t* item;
            const char* key = rinfo.fileName.c_str();
            if(cache.find(key, &item))
            {
                if(item->enabled && item->raster->sampled())
                {
                    RasterSample& sample = item->raster->getSample();
                    flags = sample.value;
                }
            }
            break;
        }
    }

    return flags;
}

/*----------------------------------------------------------------------------
 * getGmtDate
 *----------------------------------------------------------------------------*/
double GeoIndexedRaster::getGmtDate(const OGRFeature* feature, const char* field,  TimeLib::gmt_time_t& gmtDate)
{
    bzero(&gmtDate, sizeof(TimeLib::gmt_time_t));

    int i = feature->GetFieldIndex(field);
    if(i == -1)
    {
        mlog(ERROR, "Time field: %s not found, unable to get GMT date", field);
        return 0;
    }

    int year, month, day, hour, minute, second, timeZone;
    year = month = day = hour = minute = second = timeZone = 0;
    if(feature->GetFieldAsDateTime(i, &year, &month, &day, &hour, &minute, &second, &timeZone))
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

    // mlog(DEBUG, "%04d:%02d:%02d:%02d:%02d:%02d", year, month, day, hour, minute, second);
    return static_cast<double>(TimeLib::gmt2gpstime(gmtDate));
}

/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::openGeoIndex(double lon, double lat)
{
    std::string newFile;
    getIndexFile(newFile, lon, lat);

    /* Trying to open the same file? */
    if(featuresList.isempty() && newFile == indexFile)
        return;

    GDALDataset* dset = NULL;
    try
    {
        destroyFeaturesList();

        /* Open new vector data set*/
        dset = (GDALDataset *)GDALOpenEx(newFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        if (dset == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "Failed to open vector index file (%.2lf, %.2lf), file: %s:", lon, lat, newFile.c_str());

        indexFile = newFile;
        OGRLayer* layer = dset->GetLayer(0);
        CHECKPTR(layer);

        /*
         * Clone all features and store them for performance/speed of feature lookup
         */
        layer->ResetReading();
        while(OGRFeature* feature = layer->GetNextFeature())
        {
            OGRFeature* fp = feature->Clone();
            featuresList.add(fp);
            OGRFeature::DestroyFeature(feature);
        }

        cols = dset->GetRasterXSize();
        rows = dset->GetRasterYSize();


        OGREnvelope env;
        OGRErr err = layer->GetExtent(&env);
        if(err == OGRERR_NONE )
        {
            bbox.lon_min = env.MinX;
            bbox.lat_min = env.MinY;
            bbox.lon_max = env.MaxX;
            bbox.lat_max = env.MaxY;
            mlog(DEBUG, "Layer extent/bbox: (%.6lf, %.6lf), (%.6lf, %.6lf)", bbox.lon_min, bbox.lat_min, bbox.lon_max, bbox.lat_max);
        }

        GDALClose((GDALDatasetH)dset);
        mlog(DEBUG, "Loaded index file features from: %s", newFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        if(dset) GDALClose((GDALDatasetH)dset);
        destroyFeaturesList();
        throw;
    }
}


/*----------------------------------------------------------------------------
 * sampleRasters
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::sampleRasters(uint32_t cnt)
{
    /* Create additional reader threads if needed */
    createThreads(cnt);

    /* For each raster which is marked to be sampled, give it to the reader thread */
    int signaledReaders = 0;
    int i = 0;
    cacheitem_t* item;
    const char* key = cache.first(&item);
    while(key != NULL)
    {
        if(item->enabled)
        {
            reader_t* reader = readers[i++];
            reader->sync->lock();
            {
                reader->raster = item->raster;
                reader->sync->signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
                signaledReaders++;
            }
            reader->sync->unlock();
        }
        key = cache.next(&item);
    }

    /* If did not signal any reader threads, don't wait */
    if(signaledReaders == 0) return;

    /* Wait for readers to finish sampling */
    for(int j = 0; j < signaledReaders; j++)
    {
        reader_t* reader = readers[j];
        reader->sync->lock();
        {
            while(reader->raster != NULL)
                reader->sync->wait(DATA_SAMPLED, SYS_TIMEOUT);
        }
        reader->sync->unlock();
    }
}

/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
int GeoIndexedRaster::sample(double lon, double lat, double height, int64_t gps)
{
    invalidateCache();

    /* Initial call, open index file if not already opened */
    if(featuresList.isempty())
        openGeoIndex(lon, lat);

    GdalRaster::Point poi(lon, lat, height);
    if(!withinExtent(poi))
    {
        openGeoIndex(lon, lat);

        /* Check against newly opened geoindex */
        if(!withinExtent(poi))
            return 0;
    }

    if(findRasters(poi) && filterRasters(gps))
    {
        uint32_t cnt = updateCache(poi);
        sampleRasters(cnt);
    }

    return getSampledRastersCount();
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
        GeoIndexedRaster *lua_obj = (GeoIndexedRaster *)getLuaSelf(L, 1);

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
        GeoIndexedRaster *lua_obj = (GeoIndexedRaster *)getLuaSelf(L, 1);

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
        int cellSize = 0;

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
 * readingThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::readingThread(void *param)
{
    reader_t *reader = (reader_t*)param;
    bool run = true;

    while(run)
    {
        reader->sync->lock();
        {
            /* Wait for raster to work on */
            while((reader->raster == NULL) && reader->run)
                reader->sync->wait(DATA_TO_SAMPLE, SYS_TIMEOUT);

            if(reader->raster != NULL)
            {
                reader->raster->samplePOI();
                reader->raster = NULL; /* Done with this raster */
                reader->sync->signal(DATA_SAMPLED, Cond::NOTIFY_ONE);
            }

            run = reader->run;
        }
        reader->sync->unlock();
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * createThreads
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::createThreads(uint32_t cnt)
{
    uint32_t threadsNeeded = cnt;
    uint32_t threadsNow    = readers.size();

    if(threadsNeeded <= threadsNow)
        return;

    uint32_t newThreadsCnt = threadsNeeded - threadsNow;
    mlog(DEBUG, "Creating %d new threads, currentThreads: %d, neededThreads: %d, maxAlowed: %d\n",
         newThreadsCnt, threadsNow, threadsNeeded, MAX_READER_THREADS);

    if(newThreadsCnt > MAX_READER_THREADS)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR,
                               "Too many rasters to read: %d, max reading threads allowed: %d\n",
                               newThreadsCnt, MAX_READER_THREADS);
    }

    for(uint32_t i = 0; i < newThreadsCnt; i++)
    {
        reader_t* r = new reader_t;
        r->raster = NULL;
        r->run    = true;
        r->sync   = new Cond(NUM_SYNC_SIGNALS);
        r->thread = new Thread(readingThread, r);
        readers.push_back(r);
    }
    assert(readers.size() == threadsNeeded);
}

/*----------------------------------------------------------------------------
 * updateCache
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::updateCache(GdalRaster::Point& poi)
{
    uint32_t rasters2sample = 0;

    Ordering<rasters_group_t>::Iterator group_iter(*groupList);
    for(int i = 0; i < group_iter.length; i++)
    {
        const rasters_group_t& rgroup = group_iter[i].value;

        Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
        for(int j = 0; j < raster_iter.length; j++)
        {
            const raster_info_t& rinfo = raster_iter[j].value;
            const char* key            = rinfo.fileName.c_str();

            cacheitem_t* item;
            bool inCache = cache.find(key, &item);
            if(!inCache)
            {
                /* Create new cache item with raster */
                item = new cacheitem_t();
                item->raster = new GdalRaster(parms, rinfo.fileName,
                                              static_cast<double>(rinfo.gpsTime / 1000),
                                              rinfo.dataIsElevation, crscb);
            }

            item->raster->setPOI(poi);
            item->useTime = TimeLib::latchtime();
            item->enabled = true;

            if(!inCache)
            {
                /* Add new raster to dictionary */
                cache.add(key, item);
            }
            rasters2sample++;
        }
    }

    /* Maintain cache from getting too big */
    while(cache.length() > MAX_CACHED_RASTERS)
    {
        uint32_t cnt = removeOldestCacheItem();
        if(cnt == 0) break;
        else mlog(DEBUG, "Removed %u items from cache", cnt);
    }

    return rasters2sample;
}

/*----------------------------------------------------------------------------
 * invaldiateCache
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::invalidateCache(void)
{
    cacheitem_t* item;
    const char* key = cache.first(&item);
    while(key != NULL)
    {
        item->enabled = false;
        key = cache.next(&item);
    }
}

/*----------------------------------------------------------------------------
 * filterRasters
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::filterRasters(int64_t gps)
{
    /* URL and temporal filter - remove the whole raster group if one of rasters needs to be filtered out */
    if(parms->url_substring || parms->filter_time )
    {
        Ordering<rasters_group_t>::Iterator group_iter(*groupList);
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t& rgroup = group_iter[i].value;
            Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
            bool removeGroup = false;

            for(int j = 0; j < raster_iter.length; j++)
            {
                const raster_info_t& rinfo = raster_iter[j].value;

                /* URL filter */
                if(parms->url_substring)
                {
                    if(rinfo.fileName.find(parms->url_substring) == std::string::npos)
                    {
                        removeGroup = true;
                        break;
                    }
                }

                /* Temporal filter */
                if(parms->filter_time)
                {
                    if(!TimeLib::gmtinrange(rgroup.gmtDate, parms->start_time, parms->stop_time))
                    {
                        removeGroup = true;
                        break;
                    }
                }
            }

            if(removeGroup)
                groupList->remove(group_iter[i].key);
        }
    }

    /* Closest time filter - using raster group time, not individual reaster time */
    int64_t closestGps = 0;
    if(gps > 0)
        closestGps = gps;
    else if (parms->filter_closest_time)
        closestGps = TimeLib::gmt2gpstime(parms->closest_time);

    if(closestGps > 0)
    {
        int64_t minDelta = abs(std::numeric_limits<int64_t>::max() - closestGps);

        /* Find raster group with the closesest time */
        Ordering<rasters_group_t>::Iterator iter(*groupList);
        for(int i = 0; i < iter.length; i++)
        {
            int64_t gpsTime = iter[i].value.gpsTime;
            int64_t delta   = abs(closestGps - gpsTime);

            if(delta < minDelta)
                minDelta = delta;
        }

        /* Remove all groups with greater delta */
        for(int i = 0; i < iter.length; i++)
        {
            int64_t gpsTime = iter[i].value.gpsTime;
            int64_t delta   = abs(closestGps - gpsTime);

            if(delta > minDelta)
                groupList->remove(iter[i].key);
        }
    }

    return (groupList->length() > 0);
}

/*----------------------------------------------------------------------------
 * removeOldestCacheItem
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::removeOldestCacheItem(void)
{
    uint32_t removedCnt = 0;
    double max = std::numeric_limits<double>::min();

    /* Find oldest raster and it's groupId */
    cacheitem_t* oldestItem = NULL;
    cacheitem_t* item;

    const char* key = cache.first(&item);
    while(key != NULL)
    {
        if(!item->enabled)
        {
            double elapsedTime = TimeLib::latchtime() - item->useTime;
            if(elapsedTime > max)
            {
                max        = elapsedTime;
                oldestItem = item;
            }
        }
        key = cache.next(&item);
    }

    if(oldestItem == NULL)
    {
        /* Cache is empty, did not find oldest item */
        return 0;
    }

    /* Remove all cache items from the oldest group if they are not enabled */
    const std::string& oldestId = oldestItem->groupId;
    key                         = cache.first(&item);
    while(key != NULL)
    {
        if(!item->enabled && (item->groupId == oldestId))
        {
            cache.remove(key);
            delete item->raster;
            delete item;
            removedCnt++;
        }
        key = cache.next(&item);
    }

    return removedCnt;
}

/*----------------------------------------------------------------------------
 * getSampledRastersCount
 *----------------------------------------------------------------------------*/
int GeoIndexedRaster::getSampledRastersCount(void)
{
    int cnt = 0;

    /* Not all rasters in dictionary were sampled, find out how many were */
    cacheitem_t* item;
    const char *key = cache.first(&item);
    while(key != NULL)
    {
        if (item->enabled && item->raster->sampled()) cnt++;
        key = cache.next(&item);
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * destroyFeaturesList
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::destroyFeaturesList(void)
{
    int len = featuresList.length();
    if(len > 0)
    {
        for(int i = 0; i < len; i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRFeature::DestroyFeature(feature);
        }
        featuresList.clear();
    }
}
