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
const char* GeoIndexedRaster::VALUE_TAG = "Value";
const char* GeoIndexedRaster::DATE_TAG  = "datetime";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Reader Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::Reader::Reader (GeoIndexedRaster* _obj):
    obj(_obj),
    geo(NULL),
    entry(NULL),
    sync(NUM_SYNC_SIGNALS),
    run(true)
{
    thread = new Thread(GeoIndexedRaster::readerThread, this);
}

/*----------------------------------------------------------------------------
 * Reader Destructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::Reader::~Reader (void)
{
    sync.lock();
    {
        run = false; /* Set run flag to false */
        sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
    }
    sync.unlock();

    delete thread; /* delete thread waits on thread to join */

    /* geometry geo is cloned not 'newed' on GDAL heap. Use this call to free it */
    OGRGeometryFactory::destroyGeometry(geo);
}

/*----------------------------------------------------------------------------
 * Finder Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::Finder::Finder (GeoIndexedRaster* _obj):
    obj(_obj),
    geo(NULL),
    range{0, 0},
    sync(NUM_SYNC_SIGNALS),
    run(true)
{
    thread = new Thread(GeoIndexedRaster::finderThread, this);
}

/*----------------------------------------------------------------------------
 * Finder Destructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::Finder::~Finder (void)
{
    sync.lock();
    {
        run = false; /* Set run flag to false */
        sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
    }
    sync.unlock();

    delete thread; /* delete thread waits on thread to join */

    /* geometry geo is cloned not 'newed' on GDAL heap. Use this call to free it */
    OGRGeometryFactory::destroyGeometry(geo);
}

/*----------------------------------------------------------------------------
 * UnionMaker Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::UnionMaker::UnionMaker(GeoIndexedRaster* _obj, const std::vector<point_info_t>* _points):
    obj(_obj),
    range({0, 0}),
    points(_points),
    unionPolygon(NULL),
    stats({0.0, 0.0})
{
}

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
uint32_t GeoIndexedRaster::getSamples(const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param)
{
    static_cast<void>(param);

    lockSampling();
    try
    {
        GroupOrdering groupList;
        OGRPoint      ogr_point(point.x, point.y, point.z);

        ssErrors = SS_NO_ERRORS;

        /* Sample Rasters */
        if(sample(&ogr_point, gps, &groupList))
        {
            /* Populate Return Vector of Samples (slist) */
            const GroupOrdering::Iterator iter(groupList);
            for(int i = 0; i < iter.length; i++)
            {
                const rasters_group_t* rgroup = iter[i].value;
                uint32_t flags = 0;

                /* Get flags value for this group of rasters */
                if(parms->flags_file)
                    flags = getGroupFlags(rgroup);

                getGroupSamples(rgroup, slist, flags);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    /* Free Unreturned Results */
    cacheitem_t* item;
    const char* key = cache.first(&item);
    while(key != NULL)
    {
        if(item->sample)
        {
            delete item->sample;
            item->sample = NULL;
        }
        if(item->subset)
        {
            delete item->subset;
            item->subset = NULL;
        }
        key = cache.next(&item);
    }
    unlockSampling();

    return ssErrors;
}

/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSamples(const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param)
{
    static_cast<void>(param);

    // Call the base class function
    // return RasterObject::getSamples(points, sllist, param);

    lockSampling();

    const event_level_t gsLevel = INFO;

    perf_stats_t stats = {0.0, 0.0, 0.0, 0.0, 0.0};

    /* Vector of points and their associated raster groups */
    std::vector<point_groups_t> pointGroups;

    /* Vector of rasters and all points they contain */
    std::vector<unique_raster_t*> uniqueRasters;

    try
    {
        ssErrors = SS_NO_ERRORS;
        double startTime;

        /* Find raster groups for each point */
        mlog(gsLevel, "Finding rasters groups");

        /* For all points from the caller, create a vector of raster group lists */
        startTime = TimeLib::latchtime();
        for(uint32_t i = 0; i < points.size(); i++)
        {
            const point_info_t& pinfo = points[i];
            const int64_t gps = usePOItime() ? pinfo.gps : 0.0;
            OGRPoint ogr_point(pinfo.point.x, pinfo.point.y, pinfo.point.z);

            GroupOrdering* groupList = new GroupOrdering();
            findRastersParallel(&ogr_point, groupList, &points);
            filterRasters(gps, groupList);

            pointGroups.emplace_back(point_groups_t{{ogr_point, i}, groupList});
        }
        stats.findRastersTime = TimeLib::latchtime() - startTime;
        mlog(gsLevel, "time: %.3lf", stats.findRastersTime);

        /* Create vector of unique rasters. */
        mlog(gsLevel, "Finding unique rasters");
        startTime = TimeLib::latchtime();
        for(const point_groups_t& pg : pointGroups)
        {
            const GroupOrdering::Iterator iter(*pg.groupList);
            for(int64_t i = 0; i < iter.length; i++)
            {
                rasters_group_t* rgroup = iter[i].value;
                for(raster_info_t& rinfo : rgroup->infovect)
                {
                    /* Is this raster already in the list of unique rasters? */
                    bool addNewRaster = true;
                    for(unique_raster_t* ur : uniqueRasters)
                    {
                        if(ur->rinfo.fileName == rinfo.fileName)
                        {
                            /* already in unique rasters list, set pointer in rinfo to this raster */
                            rinfo.uraster = ur;
                            addNewRaster = false;
                            break;
                        }
                    }

                    if(addNewRaster)
                    {
                        unique_raster_t* ur = new unique_raster_t();
                        ur->rinfo = rinfo;
                        ur->gpsTime = rgroup->gpsTime;
                        ur->fileId = fileDictAdd(rinfo.fileName);
                        uniqueRasters.push_back(ur);

                        /* Set pointer in rinfo to this new unique raster */
                        rinfo.uraster = ur;
                    }
                }
            }
        }
        stats.findUniqueRastersTime = TimeLib::latchtime() - startTime;
        mlog(gsLevel, "rasters: %zu, time: %.3lf", uniqueRasters.size(), stats.findUniqueRastersTime);

        /*
         * For each unique raster, find the points that belong to it
         */
        mlog(gsLevel, "Finding points for unique rasters");
        startTime = TimeLib::latchtime();
        for(unique_raster_t* ur : uniqueRasters)
        {
            const std::string& rasterName = ur->rinfo.fileName;

            /* Is this raster in the group of rasters for this point? */
            for(uint32_t pointIndx = 0; pointIndx < pointGroups.size(); pointIndx++)
            {
                const point_groups_t& pg = pointGroups[pointIndx];
                const GroupOrdering::Iterator iter(*pg.groupList);
                for(int i = 0; i < iter.length; i++)
                {
                    const rasters_group_t* rgroup = iter[i].value;
                    for(const auto& ri : rgroup->infovect)
                    {
                        if(ri.fileName == rasterName)
                        {
                            /* Found rasterName in this group of rasters, add this point */
                            ur->pointSamples.push_back({pg.pointInfo, NULL, SS_NO_ERRORS});
                            break;
                        }
                    }
                }
            }
        }
        stats.findPointsForUniqueRastersTime = TimeLib::latchtime() - startTime;
        mlog(gsLevel, "time: %.3lf", stats.findPointsForUniqueRastersTime);

//HACK disable reading for now, we are debugging
#if 1
        /* Create batch reader threads */
        const uint32_t numRasters = uniqueRasters.size();

        /* Testing has shown that 20 threads performs twice as fast on a 8 core system than 100 threads.
         */
        const uint32_t maxThreads = 20;
        createBatchReaderThreads(std::min(maxThreads, numRasters));
        const uint32_t numThreads = batchReaders.length();
        mlog(gsLevel, "Sampling %u rasters with %u threads", numRasters, numThreads);

        /* Sample unique rasters utilizing numThreads */
        uint32_t currentRaster = 0;

        startTime = TimeLib::latchtime();
        while(currentRaster < numRasters)
        {
            /* Calculate how many rasters we can process in this batch */
            const uint32_t batchSize = std::min(numThreads, numRasters - currentRaster);

            /* Keep track of how many threads have been assigned work */
            uint32_t activeReaders = 0;

            /* Assign rasters to batch readers as soon as they are free */
            while(currentRaster < numRasters || activeReaders > 0)
            {
                for(uint32_t i = 0; i < batchSize; i++)
                {
                    BatchReader* breader = batchReaders[i];

                    breader->sync.lock();
                    {
                        /* If this thread is done with its previous raster, assign a new one */
                        if(breader->uraster == NULL && currentRaster < numRasters)
                        {
                            breader->uraster = uniqueRasters[currentRaster++];
                            breader->sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
                            activeReaders++;
                        }
                    }
                    breader->sync.unlock();

                    if(!isSampling())
                    {
                        /* Sampling has been stopped, stop assigning new rasters */
                        activeReaders = 0;
                        currentRaster = numRasters;
                        break;
                    }

                    /* Check if the current breader has completed its work */
                    breader->sync.lock();
                    {
                        if(breader->uraster == NULL && activeReaders > 0)
                        {
                            /* Mark one reader as free */
                            activeReaders--;
                        }
                    }
                    breader->sync.unlock();
                }

                /* Short wait before checking again to avoid busy waiting */
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        /* Wait for all batch readers to finish sampling */
        for(int32_t i = 0; i < batchReaders.length(); i++)
        {
            BatchReader* breader = batchReaders[i];

            breader->sync.lock();
            {
                while(breader->uraster != NULL)
                    breader->sync.wait(DATA_SAMPLED, SYS_TIMEOUT);
            }
            breader->sync.unlock();
        }
        stats.getSamplesTime = TimeLib::latchtime() - startTime;
        mlog(gsLevel, "time: %.3lf", stats.getSamplesTime);

        /*
         * Populate sllist with samples
         */
        if(isSampling())
        {
            mlog(gsLevel, "Populating sllist with samples");
            startTime = TimeLib::latchtime();
            for(uint32_t pointIndx = 0; pointIndx < pointGroups.size(); pointIndx++)
            {
                const point_groups_t& pg = pointGroups[pointIndx];
                const GroupOrdering::Iterator iter(*pg.groupList);

                /* Allocate a new sample list for groupList */
                sample_list_t* slist = new sample_list_t();

                for(int i = 0; i < iter.length; i++)
                {
                    const rasters_group_t* rgroup = iter[i].value;
                    uint32_t flags = 0;

                    /* Get flags value for this group of rasters */
                    if(parms->flags_file)
                        flags = getBatchGroupFlags(rgroup, pointIndx);

                    ssErrors |= getBatchGroupSamples(rgroup, slist, flags, pointIndx);
                }

                sllist.add(slist);
            }
            stats.popluateSamplesListTime = TimeLib::latchtime() - startTime;
        }
        else
        {
            /* Sampling has been stopped, return empty sllist */
            sllist.clear();
        }
#endif
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    /* Clean up */
    for(const point_groups_t& pg : pointGroups)
        delete pg.groupList;

    for(unique_raster_t* ur : uniqueRasters)
    {
        for(const point_sample_t& ps : ur->pointSamples)
            delete ps.sample;

        delete ur;
    }

    unlockSampling();

    /* Print performance stats */
    mlog(gsLevel, "Performance Stats:");
    mlog(gsLevel, "findingRasers: %12.3lf", stats.findRastersTime);
    mlog(gsLevel, "uniqueRasters: %12.3lf", stats.findUniqueRastersTime);
    mlog(gsLevel, "uinqueRPoints: %12.3lf", stats.findPointsForUniqueRastersTime);
    mlog(gsLevel, "sampling:      %12.3lf", stats.getSamplesTime);
    mlog(gsLevel, "listCreation:  %12.3lf", stats.popluateSamplesListTime);

    return ssErrors;
}

/*----------------------------------------------------------------------------
 * getSubset
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSubsets(const MathLib::extent_t& extent, int64_t gps, List<RasterSubset*>& slist, void* param)
{
    static_cast<void>(param);

    lockSampling();
    try
    {
        GroupOrdering groupList;
        OGRPolygon    poly = GdalRaster::makeRectangle(extent.ll.x, extent.ll.y, extent.ur.x, extent.ur.y);
        ssErrors = SS_NO_ERRORS;

        /* Sample Subsets */
        if(sample(&poly, gps, &groupList))
        {
            /* Populate Return Vector of Subsets (slist) */
            const GroupOrdering::Iterator iter(groupList);
            for(int i = 0; i < iter.length; i++)
            {
                const rasters_group_t* rgroup = iter[i].value;
                getGroupSubsets(rgroup, slist);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error subsetting raster: %s", e.what());
    }
    unlockSampling();

    return ssErrors;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::~GeoIndexedRaster(void)
{
    emptyFeaturesList();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * BatchReader Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::BatchReader::BatchReader (GeoIndexedRaster* _obj):
    obj(_obj),
    uraster(NULL),
    sync(NUM_SYNC_SIGNALS),
    run(true)
{
    thread = new Thread(GeoIndexedRaster::batchReaderThread, this);
}

/*----------------------------------------------------------------------------
 * BatchReader Destructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::BatchReader::~BatchReader (void)
{
    sync.lock();
    {
        run = false; /* Set run flag to false */
        sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
    }
    sync.unlock();

    delete thread; /* delete thread waits on thread to join */
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::GeoIndexedRaster(lua_State *L, GeoParms* _parms, GdalRaster::overrideCRS_t cb):
    RasterObject    (L, _parms),
    cache           (MAX_READER_THREADS),
    ssErrors        (SS_NO_ERRORS),
    crscb           (cb),
    bbox            {0, 0, 0, 0},
    rows            (0),
    cols            (0)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "dim", luaDimensions);
    LuaEngine::setAttrFunc(L, "bbox", luaBoundingBox);
    LuaEngine::setAttrFunc(L, "cell", luaCellSize);

    /* Establish Credentials */
    GdalRaster::initAwsAccess(_parms);

    /* Mark index file bbox/extent poly as empty */
    geoIndexPoly.empty();

    /* Create finder threads used to find rasters intersecting with point/polygon */
    createFinderThreads();
}


/*----------------------------------------------------------------------------
 * getBatchGroupSamples
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getBatchGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx)
{
    uint32_t errors = SS_NO_ERRORS;

    for(const auto& rinfo: rgroup->infovect)
    {
        if(strcmp(VALUE_TAG, rinfo.tag.c_str()) != 0) continue;

        /* This is the unique raster we are looking for, it cannot be NULL */
        unique_raster_t* ur = rinfo.uraster;
        assert(ur);

        /* Get the sample for this point from unique raster */
        for(const point_sample_t& ps : ur->pointSamples)
        {
            if(ps.pointInfo.index == pointIndx)
            {
                /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                if(ps.sample == NULL) break;

                /*
                * A copy of the sample must be made. The sample is owned by the unique raster,
                * and the same sample may be returned by multiple rasters that reference the same unique raster.
                */
                RasterSample* s = new RasterSample(*ps.sample);

                /* Set flags for this sample, add it to the list */
                s->flags = flags;
                slist->add(s);
                errors |= ps.ssErrors;

                /*
                 * There is only one raster with VALUE_TAG and one with FLAGS_TAG in a group.
                 * If group has other type rasters the dataset must override the getGroupFlags method.
                 */
                return errors;
            }
        }
    }

    return errors;
}

/*----------------------------------------------------------------------------
 * getBatchGroupFlags
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getBatchGroupFlags(const rasters_group_t* rgroup, uint32_t pointIndx)
{
    for(const auto& rinfo: rgroup->infovect)
    {
        if(strcmp(FLAGS_TAG, rinfo.tag.c_str()) != 0) continue;

        /* This is the unique raster we are looking for, it cannot be NULL */
        unique_raster_t* ur = rinfo.uraster;
        assert(ur);

        /* Get the sample for this point from unique raster */
        for(const point_sample_t& ps : ur->pointSamples)
        {
            if(ps.pointInfo.index == pointIndx)
            {
                /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                if(ps.sample)
                {
                    return ps.sample->value;
                }
            }
        }
    }

    return 0;
}


/*----------------------------------------------------------------------------
 * getGroupSamples
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags)
{
    for(const auto& rinfo: rgroup->infovect)
    {
        if(strcmp(VALUE_TAG, rinfo.tag.c_str()) != 0) continue;

        const char* key = rinfo.fileName.c_str();
        cacheitem_t* item;
        if(cache.find(key, &item))
        {
            RasterSample* _sample = item->sample;
            if(_sample)
            {
                _sample->flags = flags;
                slist.add(_sample);
                item->sample = NULL;
            }

            /* Get sampling/subset error status */
            ssErrors |= item->raster->getSSerror();

            /*
             * There is only one raster with VALUE_TAG and one with FLAGS_TAG in a group.
             * If group has other type rasters the dataset must override the getGroupFlags method.
             */
            break;
        }
    }
}


/*----------------------------------------------------------------------------
 * getGroupSubsets
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getGroupSubsets(const rasters_group_t* rgroup, List<RasterSubset*>& slist)
{
    for(const auto& rinfo: rgroup->infovect)
    {
        const char* key = rinfo.fileName.c_str();
        cacheitem_t* item;
        if(cache.find(key, &item))
        {
            RasterSubset* subset = item->subset;
            if(subset)
            {
                slist.add(subset);
                item->subset = NULL;
            }

            /* Get sampling/subset error status */
            ssErrors |= item->raster->getSSerror();
        }
    }
}


/*----------------------------------------------------------------------------
 * getGroupFlags
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getGroupFlags(const rasters_group_t* rgroup)
{
    for(const auto& rinfo: rgroup->infovect)
    {
        if(strcmp(FLAGS_TAG, rinfo.tag.c_str()) != 0) continue;

        cacheitem_t* item;
        const char* key = rinfo.fileName.c_str();
        if(cache.find(key, &item))
        {
            const RasterSample* _sample = item->sample;
            if(_sample)
            {
                return _sample->value;
            }
        }
    }

    return 0;
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
bool GeoIndexedRaster::openGeoIndex(const OGRGeometry* geo, const std::vector<point_info_t>* points)
{
    std::string newFile;
    getIndexFile(geo, newFile);

    /* Trying to open the same file? */
    if(!featuresList.empty() && newFile == indexFile)
        return true;

    GDALDataset* dset = NULL;
    try
    {
        emptyFeaturesList();
        geoIndexPoly.empty();

        /* Open new vector data set*/
        dset = static_cast<GDALDataset *>(GDALOpenEx(newFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL));
        if (dset == NULL)
        {
            mlog(CRITICAL, "Failed to open vector index file: %s", newFile.c_str());
            throw RunTimeException(ERROR, RTE_ERROR, "Failed to open vector index file: %s:", newFile.c_str());
        }

        indexFile = newFile;
        OGRLayer* layer = dset->GetLayer(0);
        CHECKPTR(layer);

        /* If caller provided points, use spatial filter to reduce number of features */
        if(points)
        {
            mlog(INFO, "Filtering features using spatial filter");
            mlog(INFO, "Features before spatial filter: %lld", layer->GetFeatureCount());

            OGRGeometry* filter = getBufferedPoints(points);
            if(filter)
            {
                layer->SetSpatialFilter(filter);
                OGRGeometryFactory::destroyGeometry(filter);
            }

            mlog(INFO, "Features after spatial filter:  %lld", layer->GetFeatureCount());
        }

        /*
         * Clone features and store them for performance/speed of feature lookup
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

            /* Clone feature and store it */
            OGRFeature* fp = feature->Clone();
            featuresList.push_back(fp);
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

            /* Create poly geometry for index file bbox/envelope */
            geoIndexPoly = GdalRaster::makeRectangle(bbox.lon_min, bbox.lat_min, bbox.lon_max, bbox.lat_max);
            mlog(DEBUG, "index file extent/bbox: (%.6lf, %.6lf), (%.6lf, %.6lf)", bbox.lon_min, bbox.lat_min, bbox.lon_max, bbox.lat_max);
        }

        GDALClose((GDALDatasetH)dset);
        mlog(INFO, "Loaded index file with %ld features", featuresList.size());
    }
    catch (const RunTimeException &e)
    {
        if(dset) GDALClose((GDALDatasetH)dset);
        emptyFeaturesList();
        ssErrors |= SS_INDEX_FILE_ERROR;
        return false;
    }

    return true;
}


/*----------------------------------------------------------------------------
 * sampleRasters
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::sampleRasters(OGRGeometry* geo)
{
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
            reader->sync.lock();
            {
                reader->entry = item;
                OGRGeometryFactory::destroyGeometry(reader->geo);
                reader->geo = geo->clone();
                reader->sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
                signaledReaders++;
            }
            reader->sync.unlock();
        }
        key = cache.next(&item);
    }

    /* Wait for readers to finish sampling */
    for(int j = 0; j < signaledReaders; j++)
    {
        reader_t* reader = readers[j];
        reader->sync.lock();
        {
            while(reader->entry != NULL)
                reader->sync.wait(DATA_SAMPLED, SYS_TIMEOUT);
        }
        reader->sync.unlock();
    }
}


/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::sample(OGRGeometry* geo, int64_t gps, GroupOrdering* groupList)
{
    /* Parallelized search for rasters intersecting with point/polygon */
    if(!findRastersParallel(geo, groupList))
        return false;

    /* Filter rasters - must be done in one thread (uses Lists and Orderings) */
    if(!filterRasters(gps, groupList))
        return false;

    uint32_t rasters2sample = 0;
    if(!updateCache(rasters2sample, groupList))
        return false;

    /* Create additional reader threads if needed */
    if(!createReaderThreads(rasters2sample))
        return false;

    sampleRasters(geo);

    return true;
}


/*----------------------------------------------------------------------------
 * emptyFeaturesList
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::emptyFeaturesList(void)
{
    if(featuresList.empty()) return;

    for(unsigned i = 0; i < featuresList.size(); i++)
    {
        OGRFeature* feature = featuresList[i];
        OGRFeature::DestroyFeature(feature);
    }
    featuresList.clear();
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
 * finderThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::finderThread(void *param)
{
    finder_t *finder = static_cast<finder_t*>(param);

    while(finder->run)
    {
        finder->sync.lock();
        {
            while((finder->geo == NULL) && finder->run)
                finder->sync.wait(DATA_TO_SAMPLE, SYS_TIMEOUT);
        }
        finder->sync.unlock();

        if(finder->geo)
        {
            finder->obj->findRasters(finder);

            finder->sync.lock();
            {
                OGRGeometryFactory::destroyGeometry(finder->geo);
                finder->geo = NULL;
                finder->sync.signal(DATA_SAMPLED, Cond::NOTIFY_ONE);
            }
            finder->sync.unlock();
        }
    }

    return NULL;
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
 * readerThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::readerThread(void *param)
{
    reader_t *reader = static_cast<reader_t*>(param);

    while(reader->run)
    {
        reader->sync.lock();
        {
            /* Wait for raster to work on */
            while((reader->entry == NULL) && reader->run)
                reader->sync.wait(DATA_TO_SAMPLE, SYS_TIMEOUT);
        }
        reader->sync.unlock();

        cacheitem_t* entry = reader->entry;
        if(entry != NULL)
        {
            if(GdalRaster::ispoint(reader->geo))
            {
                entry->sample = entry->raster->samplePOI(dynamic_cast<OGRPoint*>(reader->geo));
            }
            else if(GdalRaster::ispoly(reader->geo))
            {
                entry->subset = entry->raster->subsetAOI(dynamic_cast<OGRPolygon*>(reader->geo));
                if(entry->subset)
                {
                    /*
                     * Create new GeoRaster object for subsetted raster
                     * Use NULL for LuaState, using parent's causes memory corruption
                     * NOTE: cannot use RasterObject::cppCreate(parms) here, it would create
                     * new GeoIndexRaster with the same file path as parent raster.
                     */
                    entry->subset->robj = new GeoRaster(NULL,
                                                        reader->obj->parms,
                                                        entry->subset->rasterName,
                                                        entry->raster->getGpsTime(),
                                                        entry->raster->isElevation(),
                                                        entry->raster->getOverrideCRS());

                    /* GeoParms are shared with subsseted raster and other readers */
                    GeoIndexedRaster::referenceLuaObject(reader->obj->parms);
                }
            }
            entry->enabled = false; /* raster samples/subsetted */

            reader->sync.lock();
            {
                reader->entry = NULL; /* Done with this raster */
                reader->sync.signal(DATA_SAMPLED, Cond::NOTIFY_ONE);
            }
            reader->sync.unlock();
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * batchReaderThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::batchReaderThread(void *param)
{
    batch_reader_t *breader = static_cast<batch_reader_t*>(param);

    while(breader->run)
    {
        breader->sync.lock();
        {
            /* Wait for raster to work on */
            while((breader->uraster == NULL) && breader->run)
                breader->sync.wait(DATA_TO_SAMPLE, SYS_TIMEOUT);
        }
        breader->sync.unlock();

        if(breader->uraster != NULL)
        {
            unique_raster_t* ur = breader->uraster;
            GdalRaster* raster = new GdalRaster(breader->obj->parms,
                                                ur->rinfo.fileName,
                                                static_cast<double>(ur->gpsTime / 1000),
                                                ur->fileId,
                                                ur->rinfo.dataIsElevation,
                                                breader->obj->crscb);

            /* Sample all points for this raster */
            for(point_sample_t& ps : ur->pointSamples)
            {
                ps.sample = raster->samplePOI(&ps.pointInfo.point);
                ps.ssErrors |= raster->getSSerror();
            }

            delete raster;
            breader->sync.lock();
            {
                breader->uraster = NULL; /* Done with this raster and all points */
                breader->sync.signal(DATA_SAMPLED, Cond::NOTIFY_ONE);
            }
            breader->sync.unlock();
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * batchReaderThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::unionThread(void* parm)
{
    union_maker_t* um = static_cast<union_maker_t*>(parm);

    /* Create an empty geometry collection to hold all points */
    OGRGeometryCollection geometryCollection;

    const double distance  = 0.01;        /* Aproxiately 1km at equator */
    const double tolerance = distance;    /* Tolerance for simplification */

    /*
    * Create geometry collection from the bounding boxes around each point.
    * NOTE: Using buffered points is significantly more computationally
    *       expensive than bounding boxes (10x slower).
    */
    mlog(DEBUG, "Creating collection of bboxes from %d points, range: %d - %d", um->range.end - um->range.start, um->range.start, um->range.end);
    double startTime = TimeLib::latchtime();

    const uint32_t start = um->range.start;
    const uint32_t end   = um->range.end;

    for(uint32_t i = start; i < end; i++)
    {
        const point_info_t& point_info = um->points->at(i);
        const double lon = point_info.point.x;
        const double lat = point_info.point.y;

        /* Create a linear ring representing the bounding box */
        OGRLinearRing ring;
        ring.addPoint(lon - distance, lat - distance);  /* Lower-left corner  */
        ring.addPoint(lon + distance, lat - distance);  /* Lower-right corner */
        ring.addPoint(lon + distance, lat + distance);  /* Upper-right corner */
        ring.addPoint(lon - distance, lat + distance);  /* Upper-left corner  */
        ring.closeRings();

        /* Create a polygon from the ring */
        OGRPolygon* bboxPoly = new OGRPolygon();
        bboxPoly->addRing(&ring);

        /* Add the polygon to the geometry collection */
        geometryCollection.addGeometry(bboxPoly);

        /* Clean up the bounding box */
        OGRGeometryFactory::destroyGeometry(bboxPoly);
    }
    um->stats.points2polyTime = TimeLib::latchtime() - startTime;
    mlog(DEBUG, "Creating collection took %.3lf seconds", um->stats.points2polyTime);

    /*
     * Union the bounding boxes in batches to reduce computational complexity.
     * NOTE: Union call fails with large number of geometries - must use batch union.
     */
    const int batchSize = 60;  /* Sweet spot for performance based on my testing */
    OGRGeometry* unionPolygon = NULL;

    mlog(DEBUG, "Unioning point geometries using batch size: %d", batchSize);
    startTime = TimeLib::latchtime();
    for(int i = 0; i < geometryCollection.getNumGeometries(); i += batchSize)
    {
        OGRGeometry* batchUnion = NULL;
        for(int j = i; j < std::min(i + batchSize, geometryCollection.getNumGeometries()); ++j)
        {
            OGRGeometry* geo = geometryCollection.getGeometryRef(j);
            if(batchUnion == NULL)
            {
                batchUnion = geo->clone();
            }
            else
            {
                OGRGeometry* newUnion = batchUnion->Union(geo);
                OGRGeometryFactory::destroyGeometry(batchUnion);
                batchUnion = newUnion;
            }
        }

        /* Combine the batch union with the overall union polygon */
        if(batchUnion)
        {
            /* Simplify the batch union to reduce complexity */
            OGRGeometry* simplifiedBatchUnion = batchUnion->Simplify(tolerance);
            OGRGeometryFactory::destroyGeometry(batchUnion);
            batchUnion = simplifiedBatchUnion;

            if(unionPolygon == NULL)
            {
                /* Initial union polygon */
                unionPolygon = batchUnion;
            }
            else
            {
                OGRGeometry* newUnionPolygon = unionPolygon->Union(batchUnion);
                OGRGeometryFactory::destroyGeometry(unionPolygon);
                unionPolygon = newUnionPolygon;
                OGRGeometryFactory::destroyGeometry(batchUnion);
            }
        }
    }

    if(unionPolygon == NULL)
    {
        mlog(ERROR, "Unioning geometries failed");
        um->stats.unioningTime = TimeLib::latchtime() - startTime;
        return NULL;
    }

    /* Simplify the final union polygon to reduce complexity */
    OGRGeometry* simplifiedPolygon = unionPolygon->Simplify(tolerance);
    OGRGeometryFactory::destroyGeometry(unionPolygon);
    unionPolygon = simplifiedPolygon;
    if(simplifiedPolygon == NULL)
    {
        mlog(ERROR, "Failed to simplify polygon");
    }

    um->stats.unioningTime = TimeLib::latchtime() - startTime;
    mlog(DEBUG, "Unioning all geometries took %.3lf seconds", um->stats.unioningTime);


    /* Set the unioned polygon in the union maker object */
    um->unionPolygon = unionPolygon;

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * createFinderThreads
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::createFinderThreads(void)
{
    /* Finder threads are created in the constructor, this call should not fail */

    for(int i = 0; i < MAX_FINDER_THREADS; i++)
    {
        Finder* f = new Finder(this);
        finders.add(f);
    }

    return finders.length() == MAX_FINDER_THREADS;
}

/*----------------------------------------------------------------------------
 * createReaderThreads
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::createReaderThreads(uint32_t rasters2sample)
{
    const int threadsNeeded = rasters2sample;
    const int threadsNow    = readers.length();
    const int newThreadsCnt = threadsNeeded - threadsNow;

    if(threadsNeeded <= threadsNow)
        return true;

    try
    {
        for(int i = 0; i < newThreadsCnt; i++)
        {
            Reader* r = new Reader(this);
            readers.add(r);
        }
    }
    catch (const RunTimeException &e)
    {
        ssErrors |= SS_RESOURCE_LIMIT_ERROR;
        mlog(CRITICAL, "Failed to create reader threads, needed: %d, created: %d", newThreadsCnt, readers.length() - threadsNow);
    }

    return readers.length() == threadsNeeded;
}

/*----------------------------------------------------------------------------
 * createBatchReaderThreads
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::createBatchReaderThreads(uint32_t rasters2sample)
{
    const int threadsNeeded = rasters2sample;
    const int threadsNow    = batchReaders.length();
    const int newThreadsCnt = threadsNeeded - threadsNow;

    if(threadsNeeded <= threadsNow)
        return true;

    try
    {
        for(int i = 0; i < newThreadsCnt; i++)
        {
            BatchReader* r = new BatchReader(this);
            batchReaders.add(r);
        }
    }
    catch (const RunTimeException &e)
    {
        ssErrors |= SS_RESOURCE_LIMIT_ERROR;
        mlog(CRITICAL, "Failed to create batch reader threads");
    }

    return batchReaders.length() == threadsNeeded;
}

/*----------------------------------------------------------------------------
 * updateCache
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::updateCache(uint32_t& rasters2sample, const GroupOrdering* groupList)
{
    /* Mark all items in cache as not enabled */
    {
        cacheitem_t* item;
        const char* key = cache.first(&item);
        while(key != NULL)
        {
            item->enabled = false;
            key = cache.next(&item);
        }
    }

    /* Cache contains items/rasters from previous sample run */
    const GroupOrdering::Iterator group_iter(*groupList);
    for(int i = 0; i < group_iter.length; i++)
    {
        const rasters_group_t* rgroup = group_iter[i].value;
        for(const auto& rinfo : rgroup->infovect)
        {
            const char* key = rinfo.fileName.c_str();
            cacheitem_t* item;
            const bool inCache = cache.find(key, &item);
            if(!inCache)
            {
                /* Limit area of interest to the extent of vector index file */
                parms->aoi_bbox = bbox;

                /* Create new cache item with raster */
                item = new cacheitem_t;
                item->raster = new GdalRaster(parms, rinfo.fileName,
                                              static_cast<double>(rgroup->gpsTime / 1000),
                                              fileDictAdd(rinfo.fileName),
                                              rinfo.dataIsElevation, crscb);
                item->sample = NULL;
                item->subset = NULL;
                const bool status = cache.add(key, item);
                assert(status); (void)status; // cannot fail; prevents linter warnings
            }

            /* Mark as Enabled */
            item->enabled = true;
            rasters2sample++;
        }
    }

    /* Maintain cache from getting too big. */
    if(cache.length() > MAX_CACHE_SIZE)
    {
        std::vector<const char*> keys_to_remove;
        {
            cacheitem_t* item;
            const char* key = cache.first(&item);
            while(key != NULL)
            {
                if(!item->enabled)
                {
                    keys_to_remove.push_back(key);
                }
                key = cache.next(&item);
            }
        }

        /* Remove cache items found above */
        for(const char* key : keys_to_remove)
        {
            cache.remove(key);
        }
    }

    /* Check for max limit of concurent reading raster threads */
    if(rasters2sample > MAX_READER_THREADS)
    {
        ssErrors |= SS_THREADS_LIMIT_ERROR;
        mlog(ERROR, "Too many rasters to read: %d, max allowed: %d", cache.length(), MAX_READER_THREADS);
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * filterRasters
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::filterRasters(int64_t gps, GroupOrdering* groupList)
{
    /* NOTE: temporal filter is applied in openGeoIndex() */
    if(parms->url_substring || parms->filter_doy_range)
    {
        const GroupOrdering::Iterator group_iter(*groupList);
        for(int i = 0; i < group_iter.length; i++)
        {
            const rasters_group_t* rgroup = group_iter[i].value;
            bool removeGroup = false;

            for(const auto& rinfo : rgroup->infovect)
            {
                /* URL filter */
                if(parms->url_substring)
                {
                    if(rinfo.fileName.find(parms->url_substring) == std::string::npos)
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
    if(gps > 0)
    {
        /* Caller provided gps time, use it insead of time from params */
        closestGps = gps;
    }
    else if (parms->filter_closest_time)
    {
        /* Params provided closest time */
        closestGps = TimeLib::gmt2gpstime(parms->closest_time);
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
 * findRastersParallel
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::findRastersParallel(OGRGeometry* geo, GroupOrdering* groupList, const std::vector<point_info_t>* points)
{
    /* For AOI always open new index file, for POI it depends... */
    const bool openNewFile = GdalRaster::ispoly(geo) || geoIndexPoly.IsEmpty() || !geoIndexPoly.Contains(geo);
    if(openNewFile)
    {
        if(!openGeoIndex(geo, points))
            return false;

        /* Update findersRange with new featuresList */
        getThreadsRanges(findersRange, featuresList.size(), MIN_FEATURES_PER_FINDER_THREAD, MAX_FINDER_THREADS);
        mlog(DEBUG, "Using %ld finder threads to search %ld features", findersRange.size(), featuresList.size());
    }

    const uint32_t numFinders = findersRange.size();

    /* Start finder threads to find rasters intersecting with point/polygon */
    uint32_t signaledFinders = 0;

    for(uint32_t i = 0; i < numFinders; i++)
    {
        Finder* finder = finders[i];
        finder->sync.lock();
        {
            OGRGeometryFactory::destroyGeometry(finder->geo);
            finder->geo = geo->clone();
            finder->range = findersRange[i];
            finder->rasterGroups.clear();
            finder->sync.signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
            signaledFinders++;
        }
        finder->sync.unlock();
    }

    /* Wait for finder threads to finish */
    for(uint32_t i = 0; i < signaledFinders; i++)
    {
        finder_t* finder = finders[i];
        finder->sync.lock();
        {
            while(finder->geo != NULL)
                finder->sync.wait(DATA_SAMPLED, SYS_TIMEOUT);
        }
        finder->sync.unlock();
    }

    /* Combine results from all finder threads */
    for(uint32_t i = 0; i < numFinders; i++)
    {
        const Finder* finder = finders[i];
        for(uint32_t j = 0; j < finder->rasterGroups.size(); j++)
        {
            rasters_group_t* rgroup = finder->rasterGroups[j];
            groupList->add(groupList->length(), rgroup);
        }
    }

    return (!groupList->empty());
}


/*----------------------------------------------------------------------------
 * getConvexHull
 *----------------------------------------------------------------------------*/
OGRGeometry* GeoIndexedRaster::getConvexHull(const std::vector<point_info_t>* points)
{
    if(points->empty())
        return NULL;

    /* Create an empty geometry collection to hold all points */
    OGRGeometryCollection geometryCollection;

    mlog(INFO, "Creating convex hull from %zu points", points->size());

    /* Collect all points into a geometry collection */
    for(const point_info_t& point_info : *points)
    {
        const double lon = point_info.point.x;
        const double lat = point_info.point.y;

        OGRPoint* point = new OGRPoint(lon, lat);
        geometryCollection.addGeometryDirectly(point);
    }

    /* Create a convex hull that wraps around all the points */
    OGRGeometry* convexHull = geometryCollection.ConvexHull();
    if(convexHull == NULL)
    {
        mlog(ERROR, "Failed to create a convex hull around points.");
    }

    return convexHull;
}

/*----------------------------------------------------------------------------
 * getBufferedPoints
 *----------------------------------------------------------------------------*/
OGRGeometry* GeoIndexedRaster::getBufferedPoints(const std::vector<point_info_t>* points)
{
    if(points->empty())
        return NULL;

    /* Start geometry union threads */
    std::vector<Thread*> pids;
    std::vector<union_maker_t*> unionMakers;

    const uint32_t numMaxThreads = std::thread::hardware_concurrency();
    const uint32_t minPointsPerThread = 100;

    std::vector<range_t> ranges;
    getThreadsRanges(ranges, points->size(), minPointsPerThread, numMaxThreads);

    const uint32_t numThreads = ranges.size();
    const double startTime = TimeLib::latchtime();

    for(uint32_t i = 0; i < numThreads; i++)
    {
        union_maker_t* um = new union_maker_t(this, points);
        um->range = ranges[i];
        unionMakers.push_back(um);
        Thread* pid = new Thread(unionThread, um);
        pids.push_back(pid);
    }

    /* Wait for all union maker threads to finish */
    for(Thread* pid : pids)
    {
        delete pid;
    }

    /* Combine results from all union maker threads */
    OGRGeometry* unionPolygon = NULL;

    for(union_maker_t* um : unionMakers)
    {
        if(um->unionPolygon == NULL)
        {
            mlog(WARNING, "Union polygon is NULL");
            continue;
        }

        if(unionPolygon == NULL)
        {
            unionPolygon = um->unionPolygon;
        }
        else
        {
            OGRGeometry* newUnionPolygon = unionPolygon->Union(um->unionPolygon);
            OGRGeometryFactory::destroyGeometry(unionPolygon);
            unionPolygon = newUnionPolygon;
            OGRGeometryFactory::destroyGeometry(um->unionPolygon);
        }

        const double tolerance = 0.0005;  /* Simplification tolerance          */
        OGRGeometry* simplifiedPolygon = unionPolygon->Simplify(tolerance);
        OGRGeometryFactory::destroyGeometry(unionPolygon);
        unionPolygon = simplifiedPolygon;
        if(simplifiedPolygon == NULL)
        {
            mlog(ERROR, "Failed to simplify polygon");
        }
    }

    const double elapsedTime = TimeLib::latchtime() - startTime;
    mlog(INFO, "Unioning all geometries took %.3lf seconds", elapsedTime);

    /* Log stats and cleanup */
    for(uint32_t i = 0; i < unionMakers.size(); i++)
    {
        union_maker_t* um = unionMakers[i];

        mlog(INFO, "Thread[%d]: %10d - %-10d p2poly: %.3lf, unioning: %.3lf",
             i, um->range.start, um->range.end, um->stats.points2polyTime, um->stats.unioningTime);

        delete um;
    }

    return unionPolygon;
}