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
    thread = new Thread(serialReaderThread, this);
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
 * getSamples - serial sampling
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSamples(const point_info_t& pinfo, sample_list_t& slist, void* param)
{
    static_cast<void>(param);

    lockSampling();
    try
    {
        GroupOrdering groupList;
        OGRPoint      ogrPoint(pinfo.point3d.x, pinfo.point3d.y, pinfo.point3d.z);

        ssErrors = SS_NO_ERRORS;

        /* Sample Rasters */
        if(serialSample(&ogrPoint, pinfo.gps, &groupList))
        {
            /* Populate Return List of Samples (slist) */
            const GroupOrdering::Iterator iter(groupList);
            for(int i = 0; i < iter.length; i++)
            {
                const rasters_group_t* rgroup = iter[i].value;
                uint32_t flags = 0;

                /* Get flags value for this group of rasters */
                if(parms->flags_file)
                    flags = getSerialGroupFlags(rgroup);

                getSerialGroupSamples(rgroup, slist, flags);
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
        if(serialSample(&poly, gps, &groupList))
        {
            /* Populate Return Vector of Subsets (slist) */
            const GroupOrdering::Iterator iter(groupList);
            for(int i = 0; i < iter.length; i++)
            {
                const rasters_group_t* rgroup = iter[i].value;
                getSerialGroupSubsets(rgroup, slist);
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
 * getSerialGroupSamples
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getSerialGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags)
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
 * getSerialGroupSubsets
 *----------------------------------------------------------------------------*/
void GeoIndexedRaster::getSerialGroupSubsets(const rasters_group_t* rgroup, List<RasterSubset*>& slist)
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
 * getSerialGroupFlags
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSerialGroupFlags(const rasters_group_t* rgroup)
{
    for(const auto& rinfo: rgroup->infovect)
    {
        if(strcmp(FLAGS_TAG, rinfo.tag.c_str()) != 0) continue;

        cacheitem_t* item;
        const char* key = fileDict.get(rinfo.fileId);
        if(cache.find(key, &item) && !item->bandSample.empty())
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
            reader_t* reader = serialReaders[i++];
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
        reader_t* reader = serialReaders[j];
        reader->sync.lock();
        {
            while(reader->entry != NULL)
                reader->sync.wait(DATA_SAMPLED, SYS_TIMEOUT);
        }
        reader->sync.unlock();
    }
}


/*----------------------------------------------------------------------------
 * serialSample
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::serialSample(OGRGeometry* geo, int64_t gps_secs, GroupOrdering* groupList)
{
    /* Open the index file, if not already open */
    std::string ifile;;
    getIndexFile(geo, ifile);

    if(!openGeoIndex(ifile))
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

    if(!filterRasters(gps_secs, groupList, fileDict))
        return false;

    uint32_t rasters2sample = 0;
    if(!updateSerialCache(rasters2sample, groupList))
        return false;

    /* Create additional reader threads if needed */
    if(!createSerialReaderThreads(rasters2sample))
        return false;

    sampleRasters(geo);

    return true;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * serialReaderThread
 *----------------------------------------------------------------------------*/
void* GeoIndexedRaster::serialReaderThread(void *param)
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
 * createSerialReaderThreads
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::createSerialReaderThreads(uint32_t rasters2sample)
{
    const int threadsNeeded = rasters2sample;
    const int threadsNow    = serialReaders.length();
    const int newThreadsCnt = threadsNeeded - threadsNow;

    if(threadsNeeded <= threadsNow)
        return true;

    try
    {
        for(int i = 0; i < newThreadsCnt; i++)
        {
            Reader* r = new Reader(this);
            serialReaders.add(r);
        }
    }
    catch (const RunTimeException &e)
    {
        ssErrors |= SS_RESOURCE_LIMIT_ERROR;
        mlog(CRITICAL, "Failed to create reader threads, needed: %d, created: %d", newThreadsCnt, serialReaders.length() - threadsNow);
    }

    return serialReaders.length() == threadsNeeded;
}

/*----------------------------------------------------------------------------
 * updateSerialCache
 *----------------------------------------------------------------------------*/
bool GeoIndexedRaster::updateSerialCache(uint32_t& rasters2sample, const GroupOrdering* groupList)
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
                                              static_cast<double>(rgroup->gpsTime),
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