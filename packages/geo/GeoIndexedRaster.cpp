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

const double GeoIndexedRaster::TOLERANCE = 0.01;  /* Tolerance for simplification */

const char* GeoIndexedRaster::FLAGS_TAG = "Fmask";
const char* GeoIndexedRaster::VALUE_TAG = "Value";
const char* GeoIndexedRaster::DATE_TAG  = "datetime";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * PointSample Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::PointSample::PointSample(const OGRPoint& _point, int64_t _pointIndex):
    point(_point), pointIndex(_pointIndex), ssErrors(SS_NO_ERRORS) {}


/*----------------------------------------------------------------------------
 * PointSample Copy Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::PointSample::PointSample(const PointSample& ps):
    point(ps.point), pointIndex(ps.pointIndex), bandSample(ps.bandSample), ssErrors(ps.ssErrors)
{
    bandSampleReturned.resize(ps.bandSampleReturned.size());

    for (size_t i = 0; i < ps.bandSampleReturned.size(); ++i)
    {
        if(ps.bandSampleReturned[i])
        {
            /* Create a new atomic<bool> with the value loaded from the original */
            bandSampleReturned[i] = std::make_unique<std::atomic<bool>>(ps.bandSampleReturned[i]->load());
        }
        else
        {
            bandSampleReturned[i] = NULL;
        }
    }
}

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
    thread = new Thread(readerThread, this);
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

/*----------------------------------------------------------------------------
 * SampleCollector Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::SampleCollector::SampleCollector(GeoIndexedRaster* _obj, const std::vector<point_groups_t>& _pointsGroups):
    obj(_obj),
    pGroupsRange({0, 0}),
    pointsGroups(_pointsGroups),
    ssErrors(SS_NO_ERRORS)
{
}

/*----------------------------------------------------------------------------
 * GroupsFinder Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::GroupsFinder::GroupsFinder(GeoIndexedRaster* _obj, const std::vector<point_info_t>* _points):
    obj(_obj),
    pointsRange({0, 0}),
    points(_points)
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
 * getSamples - serial sampling
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSamples(const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param)
{
    static_cast<void>(param);

    lockSampling();
    try
    {
        GroupOrdering groupList;
        OGRPoint      ogrPoint(point.x, point.y, point.z);

        ssErrors = SS_NO_ERRORS;

        /* Sample Rasters */
        if(sample(&ogrPoint, gps, &groupList))
        {
            /* Populate Return List of Samples (slist) */
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

        /* Update file dictionary */
        fileDictSetSamples(&slist);
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
        for(uint32_t i = 0; i < item->bandSample.size(); i++)
        {
            if(item->bandSample[i])
            {
                delete item->bandSample[i];
                item->bandSample[i] = NULL;
            }
        }

        for(uint32_t i = 0; i < item->bandSubset.size(); i++)
        {
            if(item->bandSubset[i])
            {
                delete item->bandSubset[i];
                item->bandSubset[i] = NULL;
            }
        }
        key = cache.next(&item);
    }
    unlockSampling();

    return ssErrors;
}

/*----------------------------------------------------------------------------
 * getSamples - batch sampling
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSamples(const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param)
{
    static_cast<void>(param);

    lockSampling();

    perfStats.clear();
    cache.clear();       /* Clear cache used by serial sampling */
    fileDict.clear();     /* Start with empty file dictionary    */

    /* Vector of points and their associated raster groups */
    std::vector<point_groups_t> pointsGroups;

    /* Vector of rasters and all points they contain */
    std::vector<unique_raster_t*> uniqueRasters;

    try
    {
        ssErrors = SS_NO_ERRORS;

        /* Open the index file for all points */
        if(!openGeoIndex(NULL, &points))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Error opening index file");
        }

        {
            /* Rasters to points map */
            raster_points_map_t rasterToPointsMap;

            /* For all points create a vector of raster group lists */
            if(!findAllGroups(&points, pointsGroups, rasterToPointsMap))
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Error creating groups");
            }

            /* For all points create a vector of unique rasters */
            if(!findUniqueRasters(uniqueRasters, pointsGroups, rasterToPointsMap))
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Error finding unique rasters");
            }

            /* rastersToPointsMap is no longer needed */
        }

        /* Sample all unique rasters */
        if(!sampleUniqueRasters(uniqueRasters))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Error sampling unique rasters");
        }

        /* Populate sllist with samples */
        if(!collectSamples(pointsGroups, sllist))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Error collecting samples");
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    /* Clean up pointsGroups */
    for(const point_groups_t& pg : pointsGroups)
        delete pg.groupList;

    /* Clean up unique rasters */
    for(unique_raster_t* ur : uniqueRasters)
    {
        for(const point_sample_t& ps : ur->pointSamples)
        {
            /* Delete samples which have not been returned (quality masks, etc) */
            for(size_t i = 0; i < ps.bandSample.size(); i++)
            {
                if(!ps.bandSampleReturned[i]->load())
                {
                    delete ps.bandSample[i];
                }
            }
        }

        delete ur;
    }

    unlockSampling();

    /* Print performance stats */
    perfStats.log(INFO);

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

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * BatchReader Constructor
 *----------------------------------------------------------------------------*/
GeoIndexedRaster::BatchReader::BatchReader(GeoIndexedRaster* _obj) :
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
GeoIndexedRaster::BatchReader::~BatchReader(void)
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
GeoIndexedRaster::GeoIndexedRaster(lua_State *L, RequestFields* rqst_parms, const char* key, GdalRaster::overrideCRS_t cb):
    RasterObject    (L, rqst_parms, key),
    cache           (MAX_READER_THREADS),
    ssErrors        (SS_NO_ERRORS),
    crscb           (cb),
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
        for(point_sample_t& ps : ur->pointSamples)
        {
            if(ps.pointIndex == pointIndx)
            {
                for(size_t i = 0; i < ps.bandSample.size(); i++)
                {
                    /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                    if(ps.bandSample[i] == NULL) continue;;

                    RasterSample* s;
                    if(!ps.bandSampleReturned[i]->exchange(true))
                    {
                        s = ps.bandSample[i];
                    }
                    else
                    {
                        /* Sample has already been returned, must create a copy */
                        s = new RasterSample(*ps.bandSample[i]);
                    }

                    /* Set time for this sample */
                    s->time = rgroup->gpsTime / 1000;

                    /* Set flags for this sample, add it to the list */
                    s->flags = flags;
                    slist->add(s);
                    errors |= ps.ssErrors;
                }

                /*
                 * This function assumes that there is only one raster with VALUE_TAG in a group.
                 * If group has other value rasters the dataset must override this function.
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
            if(ps.pointIndex == pointIndx)
            {
                /*
                 * This function assumes that there is only one raster with FLAGS_TAG in a group.
                 * The flags value must be in the first band.
                 * If these assumptions are not met the dataset must override this function.
                 */
                RasterSample* s = ps.bandSample[0];

                /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                if(s == NULL) break;;

                return s->value;
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

        const char* key = fileDict.get(rinfo.fileId);
        cacheitem_t* item;
        if(cache.find(key, &item))
        {
            for(uint32_t i = 0; i < item->bandSample.size(); i++)
            {
                RasterSample* _sample = item->bandSample[i];
                if(_sample)
                {
                    _sample->flags = flags;
                    slist.add(_sample);
                    item->bandSample[i] = NULL;
                }
            }

            /* Get sampling/subset error status */
            ssErrors |= item->raster->getSSerror();

           /*
            * This function assumes that there is only one raster with VALUE_TAG in a group.
            * If group has other value rasters the dataset must override this function.
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
        const char* key = fileDict.get(rinfo.fileId);
        cacheitem_t* item;
        if(cache.find(key, &item))
        {
            for(uint32_t i = 0; i < item->bandSubset.size(); i++)
            {
                RasterSubset* subset = item->bandSubset[i];
                if(subset)
                {
                    slist.add(subset);
                    item->bandSubset[i] = NULL;
                }
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
        const char* key = fileDict.get(rinfo.fileId);
        if(cache.find(key, &item))
        {
            /*
             * This function assumes that there is only one raster with FLAGS_TAG in a group.
             * The flags value must be in the first band.
             * If these assumptions are not met the dataset must override this function.
             */
            const RasterSample* _sample = item->bandSample[0];
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
    getIndexFile(geo, newFile, points);

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
            throw RunTimeException(ERROR, RTE_ERROR, "Failed to open vector index file: %s:", newFile.c_str());
        }

        indexFile = newFile;
        OGRLayer* layer = dset->GetLayer(0);
        CHECKPTR(layer);

        /* If caller provided points, use spatial filter to reduce number of features */
        if(points)
        {
            applySpatialFilter(layer, points);
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
    /* Open the index file, if not already open */
    if(!openGeoIndex(geo, NULL))
        return false;

    /* Find rasters that intersect with the geometry */
    std::vector<OGRFeature*> foundFeatures;

    /* Query the R-tree with the OGRPoint and get the result features */
    geoRtree.query(geo, foundFeatures);

    raster_finder_t finder(geo, &foundFeatures, fileDict);
    if(!findRasters(&finder))
        return false;

    /* Copy finder's raster groups to groupList */
    for(uint32_t j = 0; j < finder.rasterGroups.size(); j++)
    {
        rasters_group_t* rgroup = finder.rasterGroups[j];
        groupList->add(groupList->length(), rgroup);
    }

    if(!filterRasters(gps, groupList, fileDict))
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
            GdalRaster* raster = entry->raster;

            try
            {
                CHECKPTR(raster);

                /* Open raster so we can get inner bands from it */
                raster->open();

                std::vector<int> bands;
                reader->obj->getInnerBands(raster, bands);

                if(GdalRaster::ispoint(reader->geo))
                {
                    /* Sample raster bands */
                    const bool oneBand = bands.size() == 1;
                    if(oneBand)
                    {
                        OGRPoint* p = (dynamic_cast<OGRPoint*>(reader->geo));
                        RasterSample* sample = raster->samplePOI(p, bands[0]);
                        entry->bandSample.push_back(sample);
                    }
                    else
                    {
                        /* Multiple bands */
                        for(const int bandNum : bands)
                        {
                            /* Use local copy of point, it will be projected in samplePOI. We do not want to project it again */
                            OGRPoint* p = (dynamic_cast<OGRPoint*>(reader->geo));
                            OGRPoint point(*p);

                            RasterSample* sample = raster->samplePOI(&point, bandNum);
                            entry->bandSample.push_back(sample);
                            mlog(DEBUG, "Band: %d, %s", bandNum, sample ? sample->toString().c_str() : "NULL");
                        }
                    }
                }
                else if(GdalRaster::ispoly(reader->geo))
                {
                    /* Subset raster bands */
                    for(const int bandNum : bands)
                    {
                        /* No need to use local copy of polygon, subsetAOI will use it's envelope and not project it */
                        RasterSubset* subset = raster->subsetAOI(dynamic_cast<OGRPolygon*>(reader->geo), bandNum);
                        if(subset)
                        {
                            /*
                             * Create new GeoRaster object for subsetted raster
                             * Use NULL for LuaState, using parent's causes memory corruption
                             * NOTE: cannot use RasterObject::cppCreate(parms) here, it would create
                             * new GeoIndexRaster with the same file path as parent raster.
                             */
                            subset->robj = new GeoRaster(NULL,
                                                        reader->obj->rqstParms,
                                                        reader->obj->samplerKey,
                                                        subset->rasterName,
                                                        raster->getGpsTime(),
                                                        raster->getElevationBandNum(),
                                                        raster->getFLagsBandNum(),
                                                        raster->getOverrideCRS());

                            entry->bandSubset.push_back(subset);

                            /* RequestFields are shared with subsseted raster and other readers */
                            GeoIndexedRaster::referenceLuaObject(reader->obj->rqstParms);
                        }
                    }
                }
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "%s", e.what());
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
            GdalRaster* raster = NULL;

            try
            {
                raster = new GdalRaster(breader->obj->parms,
                                        breader->obj->fileDict.get(ur->rinfo->fileId),
                                        0,                     /* Sample collecting code will set it to group's gpsTime */
                                        ur->rinfo->fileId,
                                        ur->rinfo->elevationBandNum,
                                        ur->rinfo->flagsBandNum,
                                        breader->obj->crscb);

                CHECKPTR(raster);

                /* Open raster so we can get inner bands from it */
                raster->open();

                std::vector<int> bands;
                breader->obj->getInnerBands(raster, bands);

                /* Sample all points for this raster */
                for(point_sample_t& ps : ur->pointSamples)
                {
                    /* Sample raster bands */
                    const bool oneBand = bands.size() == 1;
                    if(oneBand)
                    {
                        RasterSample* sample = raster->samplePOI(&ps.point, bands[0]);
                        ps.bandSample.push_back(sample);
                        ps.bandSampleReturned.emplace_back(std::make_unique<std::atomic<bool>>(false));
                    }
                    else
                    {
                        /* Multiple bands */
                        for(const int bandNum : bands)
                        {
                            /* Use local copy of point, it will be projected in samplePOI. We do not want to project it again */
                            OGRPoint point(ps.point);

                            RasterSample* sample = raster->samplePOI(&point, bandNum);
                            ps.bandSample.push_back(sample);
                            ps.bandSampleReturned.emplace_back(std::make_unique<std::atomic<bool>>(false));
                            ps.ssErrors |= raster->getSSerror();
                        }
                    }
                }
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "%s", e.what());
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
 * groupsFinderThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::groupsFinderThread(void *param)
{
    groups_finder_t* gf = static_cast<groups_finder_t*>(param);

    /* Thread must initialize GEOS context */
    GEOSContextHandle_t threadGeosContext = GeoRtree::init();

    const uint32_t start = gf->pointsRange.start;
    const uint32_t end = gf->pointsRange.end;

    mlog(DEBUG, "Finding groups for points range: %u - %u", start, end);

    for(uint32_t i = start; i < end; i++)
    {
        if(!gf->obj->sampling())
        {
            mlog(WARNING, "Sampling has been stopped, exiting groups finder thread");
            break;
        }

        const point_info_t& pinfo = gf->points->at(i);
        const OGRPoint ogrPoint(pinfo.point.x, pinfo.point.y, pinfo.point.z);

        /* Query the R-tree with the OGRPoint and get the result features */
        std::vector<OGRFeature*> foundFeatures;
        gf->obj->geoRtree.query(&ogrPoint, threadGeosContext, foundFeatures);
        // mlog(DEBUG, "Found %zu features for point %u", foundFeatures.size(), i);

        /* Clone found features since OGRFeature is not thread safe */
        std::vector<OGRFeature*> threadFeatures;
        threadFeatures.reserve(foundFeatures.size());

        for(OGRFeature* feature : foundFeatures)
        {
            threadFeatures.push_back(feature->Clone());
        }

        /* Set finder for the found features */
        RasterFinder finder(&ogrPoint, &threadFeatures, gf->threadFileDict);

        /* Find rasters intersecting with ogrPoint */
        gf->obj->findRasters(&finder);

        /* Destroy cloned features */
        for(OGRFeature* feature : threadFeatures)
        {
            OGRFeature::DestroyFeature(feature);
        }

        /* Copy rasterGroups from finder to local groupList */
        GroupOrdering* groupList = new GroupOrdering();
        for(rasters_group_t* rgroup : finder.rasterGroups)
        {
            groupList->add(groupList->length(), rgroup);
        }

        /* Filter rasters based on POI time */
        const int64_t gps = gf->obj->usePOItime() ? pinfo.gps : 0.0;
        gf->obj->filterRasters(gps, groupList, gf->threadFileDict);

        /* Add found rasters which passed the filter to pointsGroups */
        gf->pointsGroups.emplace_back(point_groups_t{ogrPoint, i, groupList});

        /* Add raster file names from this groupList to raster to points map */
        const GroupOrdering::Iterator iter(*groupList);
        for(int j = 0; j < iter.length; j++)
        {
            const rasters_group_t* rgroup = iter[j].value;
            for(const raster_info_t& rinfo : rgroup->infovect)
            {
                const char* fileName = gf->threadFileDict.get(rinfo.fileId);
                gf->rasterToPointsMap[fileName].insert(i);
            }
        }
    }

    mlog(DEBUG, "Found %zu point groups for range: %u - %u", gf->pointsGroups.size(), start, end);

    /* Thread must initialize GEOS context */
    GeoRtree::deinit(threadGeosContext);

    return NULL;
}

/*----------------------------------------------------------------------------
 * samplesCollectThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::samplesCollectThread(void* param)
{
    sample_collector_t* sc = static_cast<sample_collector_t*>(param);

    const uint32_t start = sc->pGroupsRange.start;
    const uint32_t end   = sc->pGroupsRange.end;

    mlog(DEBUG, "Collecting samples for range: %u - %u", start, end);

    u_int32_t numSamples = 0;
    for(uint32_t pointIndx = start; pointIndx < end; pointIndx++)
    {
        if(!sc->obj->sampling())
        {
            mlog(WARNING, "Sampling has been stopped, exiting samples collect thread");
            break;
        }

        const point_groups_t& pg = sc->pointsGroups[pointIndx];

        /* Allocate a new sample list for groupList */
        sample_list_t* slist = new sample_list_t();

        const GroupOrdering::Iterator iter(*pg.groupList);
        for(int i = 0; i < iter.length; i++)
        {
            const rasters_group_t* rgroup = iter[i].value;
            uint32_t flags = 0;

            /* Get flags value for this group of rasters */
            if(sc->obj->parms->flags_file)
                flags = getBatchGroupFlags(rgroup, pointIndx);

            sc->ssErrors |= sc->obj->getBatchGroupSamples(rgroup, slist, flags, pointIndx);
            numSamples += slist->length();
        }

        sc->slvector.push_back(slist);
    }

    mlog(DEBUG, "Collected %u samples for range: %u - %u", numSamples, start, end);

    return NULL;
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
            const char* key = fileDict.get(rinfo.fileId);
            cacheitem_t* item;
            const bool inCache = cache.find(key, &item);
            if(!inCache)
            {
                /* Create new cache item with raster
                    note use of bbox in construcutor - it limits area
                    of interest to the extent of vector index file */
                item = new cacheitem_t;
                item->raster = new GdalRaster(parms,
                                              key,
                                              static_cast<double>(rgroup->gpsTime / 1000),
                                              rinfo.fileId,
                                              rinfo.elevationBandNum,
                                              rinfo.flagsBandNum,
                                              crscb,
                                              &bbox);
                const bool status = cache.add(key, item);
                assert(status); (void)status; // cannot fail; prevents linter warnings
            }

            /* Clear from previous run */
            item->bandSample.clear();
            item->bandSample.clear();

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
bool GeoIndexedRaster::filterRasters(int64_t gps, GroupOrdering* groupList, RasterFileDictionary& dict)
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
        return NULL;
    }

    /* Add a buffer around the convex hull to avoid missing edge points */
    OGRGeometry* bufferedConvexHull = convexHull->Buffer(TOLERANCE);
    if(bufferedConvexHull)
    {
        OGRGeometryFactory::destroyGeometry(convexHull);
    }

    return bufferedConvexHull;
}

/*----------------------------------------------------------------------------
 * applySpatialFilter
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::applySpatialFilter(OGRLayer* layer, const std::vector<point_info_t>* points)
{
    mlog(INFO, "Features before spatial filter: %lld", layer->GetFeatureCount());

    const double startTime = TimeLib::latchtime();

    /* Buffered points generates more detailed filter polygon but is much slower than
     * convex hull, especially for large number of points.
     */
    OGRGeometry* filter = getConvexHull(points);
    if(filter != NULL)
    {
        layer->SetSpatialFilter(filter);
        OGRGeometryFactory::destroyGeometry(filter);
    }
    perfStats.spatialFilterTime = TimeLib::latchtime() - startTime;

    mlog(INFO, "Features after spatial filter: %lld", layer->GetFeatureCount());
    mlog(DEBUG, "Spatial filter time: %.3lf", perfStats.spatialFilterTime);
}

/*----------------------------------------------------------------------------
 * findAllGroups
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::findAllGroups(const std::vector<point_info_t>* points,
                                     std::vector<point_groups_t>& pointsGroups,
                                     raster_points_map_t& rasterToPointsMap)
{
    /* Do not find groups if sampling stopped */
    if(!sampling()) return true;

    bool status = false;
    const double startTime = TimeLib::latchtime();

    try
    {
        /* Start rasters groups finder threads */
        std::vector<Thread*> pids;
        std::vector<GroupsFinder*> rgroupFinders;

        const uint32_t numMaxThreads = std::thread::hardware_concurrency();
        const uint32_t minPointsPerThread = 100;

        mlog(INFO, "Finding rasters groups for all points with %u threads", numMaxThreads);

        std::vector<range_t> pointsRanges;
        getThreadsRanges(pointsRanges, points->size(), minPointsPerThread, numMaxThreads);
        const uint32_t numThreads = pointsRanges.size();

        for(uint32_t i = 0; i < numThreads; i++)
        {
            GroupsFinder* gf = new GroupsFinder(this, points);
            gf->pointsRange = pointsRanges[i];
            rgroupFinders.push_back(gf);
            Thread* pid = new Thread(groupsFinderThread, gf);
            pids.push_back(pid);
        }

        /* Wait for all groups finder threads to finish */
        for(Thread* pid : pids)
        {
            delete pid;
        }

        mlog(INFO, "All groups finders time: %lf", TimeLib::latchtime() - startTime);

        /* Merge the pointGroups from each thread */
        mlog(INFO, "Merging point groups from all threads");
        for(GroupsFinder* gf : rgroupFinders)
        {
            /* Threads used local file dictionary, combine them and update fileId */
            for(const point_groups_t& pg: gf->pointsGroups)
            {
                const GroupOrdering::Iterator iter(*pg.groupList);
                for (int64_t i = 0; i < iter.length; i++)
                {
                    rasters_group_t *rgroup = iter[i].value;
                    for (raster_info_t &rinfo : rgroup->infovect)
                    {
                        /* Get file from thread file dictionary */
                        const std::string fileName = gf->threadFileDict.get(rinfo.fileId);

                        /* Add to main file dictionary */
                        rinfo.fileId = fileDict.add(fileName);
                    }
                }

                pointsGroups.push_back(pg);
            }

            /* Merge the rasterToPointsMap for each thread */
            for (const raster_points_map_t::value_type &pair : gf->rasterToPointsMap)
            {
                rasterToPointsMap[pair.first].insert(pair.second.begin(), pair.second.end());
            }

            delete gf;
        }

        /* Verify that the number of points groups is the same as the number of points */
        if(pointsGroups.size() != points->size())
        {
            mlog(ERROR, "Number of points groups: %zu does not match number of points: %zu", pointsGroups.size(), points->size());
            throw RunTimeException(CRITICAL, RTE_ERROR, "Number of points groups does not match number of points");
        }

        /* Reduce memory usage */
        pointsGroups.shrink_to_fit();
        rasterToPointsMap.rehash(rasterToPointsMap.size());

        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating groups: %s", e.what());
    }

    perfStats.findRastersTime = TimeLib::latchtime() - startTime;
    return status;
}


/*----------------------------------------------------------------------------
 * findUniqueRasters
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::findUniqueRasters(std::vector<unique_raster_t*>& uniqueRasters,
                                         const std::vector<point_groups_t>& pointsGroups,
                                         raster_points_map_t& rasterToPointsMap)
{
    /* Do not find unique rasters if sampling stopped */
    if(!sampling()) return true;

    bool status = false;
    const double startTime = TimeLib::latchtime();

    try
    {
        /* Map to track the index of each unique raster in the uniqueRasters vector */
        std::unordered_map<std::string, size_t> fileIndexMap;

        /* Create vector of unique rasters. */
        mlog(DEBUG, "Finding unique rasters");
        for(const point_groups_t& pg : pointsGroups)
        {
            const GroupOrdering::Iterator iter(*pg.groupList);
            for(int64_t i = 0; i < iter.length; i++)
            {
                rasters_group_t* rgroup = iter[i].value;
                for(raster_info_t& rinfo : rgroup->infovect)
                {
                    /* Is this raster already in the list of unique rasters? */
                    const std::string fileName = fileDict.get(rinfo.fileId);
                    auto it = fileIndexMap.find(fileName);
                    if(it != fileIndexMap.end())
                    {
                        /* Raster is already in the vector of unique rasters, get index from map and update uraster pointer */
                        rinfo.uraster = uniqueRasters[it->second];
                    }
                    else
                    {
                        /* Raster is not in the vector of unique rasters */
                        unique_raster_t* ur = new unique_raster_t(&rinfo);
                        uniqueRasters.push_back(ur);

                        /* Set pointer in rinfo to new unique raster */
                        rinfo.uraster = ur;

                        /* Update index map */
                        fileIndexMap[fileName] = uniqueRasters.size() - 1;
                    }
                }
            }
        }

        /* For each unique raster, find the points that belong to it */
        mlog(DEBUG, "Finding points for unique rasters");
        for(unique_raster_t* ur : uniqueRasters)
        {
            auto it = rasterToPointsMap.find(fileDict.get(ur->rinfo->fileId));
            if(it != rasterToPointsMap.end())
            {
                for(const uint32_t pointIndx : it->second)
                {
                    const point_groups_t& pg = pointsGroups[pointIndx];
                    ur->pointSamples.emplace_back(pg.point, pg.pointIndex);
                }
                ur->pointSamples.shrink_to_fit();
            }
        }

        /* Reduce memory usage */
        uniqueRasters.shrink_to_fit();
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating groups: %s", e.what());
    }

    perfStats.findUniqueRastersTime = TimeLib::latchtime() - startTime;
    mlog(INFO, "Unique rasters time: %lf", perfStats.findUniqueRastersTime);

    return status;
}


/*----------------------------------------------------------------------------
 * sampleUniqueRasters
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::sampleUniqueRasters(const std::vector<unique_raster_t*>& uniqueRasters)
{
    /* Do not sample rasters if sampling stopped */
    if(!sampling()) return true;

    bool status = false;
    const double startTime = TimeLib::latchtime();

    try
    {
        /* Testing has shown that 20 threads performs twice as fast on a 8 core system than 50 or 100 threads. */
        const uint32_t maxThreads = 20;

        /* Create batch reader threads */
        const uint32_t numRasters = uniqueRasters.size();
        createBatchReaderThreads(std::min(maxThreads, numRasters));

        const uint32_t numThreads = batchReaders.length();
        mlog(INFO, "Sampling %u rasters with %u threads", numRasters, numThreads);

        /* Sample unique rasters utilizing numThreads */
        uint32_t currentRaster = 0;

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

                    if(!sampling())
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

        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating groups: %s", e.what());
    }

    perfStats.samplesTime = TimeLib::latchtime() - startTime;
    mlog(INFO, "Done Sampling, time: %lf", perfStats.samplesTime);
    return status;
}


/*----------------------------------------------------------------------------
 * collectSamples
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::collectSamples(const std::vector<point_groups_t>& pointsGroups, List<sample_list_t*>& sllist)
{
    /* Do not collect samples if sampling stopped */
    if(!sampling()) return true;

    const double start = TimeLib::latchtime();

    /* Sanity check for pointsGroups, internal pointIndex should be the same as the index in pointsGroups vector */
    assert(std::all_of(pointsGroups.cbegin(), pointsGroups.cend(),
                       [&pointsGroups](const point_groups_t& pg) { return pg.pointIndex == &pg - pointsGroups.data(); }));

    /* Start sample collection threads */
    std::vector<Thread*> pids;
    std::vector<SampleCollector*> sampleCollectors;

    const uint32_t numMaxThreads = std::thread::hardware_concurrency();
    const uint32_t minPointGroupsPerThread = 100;

    std::vector<range_t> pGroupRanges;
    getThreadsRanges(pGroupRanges, pointsGroups.size(), minPointGroupsPerThread, numMaxThreads);
    const uint32_t numThreads = pGroupRanges.size();

    mlog(INFO, "Collecting samples for %zu points with %u threads", pointsGroups.size(), numThreads);

    for(uint32_t i = 0; i < numThreads; i++)
    {
        SampleCollector* sc = new SampleCollector(this, pointsGroups);
        sc->pGroupsRange = pGroupRanges[i];
        sampleCollectors.push_back(sc);
        Thread* pid = new Thread(samplesCollectThread, sc);
        pids.push_back(pid);
    }

    /* Wait for all sample collection threads to finish */
    for(Thread* pid : pids)
    {
        delete pid;
    }

    /* Merge sample lists from all sample collection threads */
    const double mergeStart = TimeLib::latchtime();
    mlog(DEBUG, "Merging sample lists");
    for(SampleCollector* sc : sampleCollectors)
    {
        const std::vector<sample_list_t*>& slvector = sc->slvector;
        for(sample_list_t* slist : slvector)
        {
            /* Update file dictionary */
            fileDictSetSamples(slist);

            sllist.add(slist);
        }
        ssErrors |= sc->ssErrors;
        delete sc;
    }
    mlog(DEBUG, "Merged %d sample lists, time: %lf", sllist.length(), TimeLib::latchtime() - mergeStart);

    perfStats.collectSamplesTime = TimeLib::latchtime() - start;
    mlog(INFO, "Populated sllist with %d lists of samples, time: %lf", sllist.length(), perfStats.collectSamplesTime);

    return true;
}