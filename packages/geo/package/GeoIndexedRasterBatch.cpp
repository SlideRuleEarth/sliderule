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

#include <algorithm> // Required for std::all_of

#include "GeoRaster.h"
#include "GeoIndexedRaster.h"

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
 * getSamples - batch sampling
 *----------------------------------------------------------------------------*/
uint32_t GeoIndexedRaster::getSamples(const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param)
{
    static_cast<void>(param);

    lockSampling();

    perfStats.clear();
    cache.clear();       /* Clear cache used by serial sampling */
    fileDict.clear();    /* Start with empty file dictionary    */

    /* Vector of points and their associated raster groups */
    std::vector<point_groups_t> pointsGroups;

    /* Vector of rasters and all points they contain */
    std::vector<unique_raster_t*> uniqueRasters;

    try
    {
        ssErrors = SS_NO_ERRORS;

        /* Get index file for the points */
        std::string ifile;
        getIndexFile(&points, ifile);

        /* Create a convex hull that wraps around all the points, used for spatial filter */
        OGRGeometry* filter = getConvexHull(&points);

        /* Open the index file */
        const bool indexOpenedOk = openGeoIndex(ifile, filter);

        /* Clean up convex hull */
        OGRGeometryFactory::destroyGeometry(filter);

        if(!indexOpenedOk)
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

                    RasterSample* sample;
                    if(!ps.bandSampleReturned[i]->exchange(true))
                    {
                        sample = ps.bandSample[i];
                    }
                    else
                    {
                        /* Sample has already been returned, must create a copy */
                        sample = new RasterSample(*ps.bandSample[i]);
                    }

                    /* Set time for this sample */
                    sample->time = rgroup->gpsTime;

                    /* Set flags for this sample, add it to the list */
                    sample->flags = flags;
                    slist->add(sample);
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
                RasterSample* sample = NULL;

                /* bandSample can be empty if raster failed to open */
                if(!ps.bandSample.empty())
                {
                    sample = ps.bandSample[0];
                }


                /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                if(sample == NULL) break;;

                return sample->value;
            }
        }
    }

    return 0;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

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
                                        breader->obj->crscb,
                                        &breader->obj->bbox);

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
        const OGRPoint ogrPoint(pinfo.point3d.x, pinfo.point3d.y, pinfo.point3d.z);

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

#if 0
        /* For each unique raster, print its name and points in it */
        mlog(DEBUG, "Unique rasters:");
        for(unique_raster_t* ur : uniqueRasters)
        {
            mlog(DEBUG, "Unique raster: %s", fileDict.get(ur->rinfo->fileId));
            for(point_sample_t& ps : ur->pointSamples)
            {
                mlog(DEBUG, "Point index: %ld, (%.2lf, %.2lf)", ps.pointIndex, ps.point.getX(), ps.point.getY());
            }
        }
#endif

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
    for(const point_info_t& pinfo : *points)
    {
        const double lon = pinfo.point3d.x;
        const double lat = pinfo.point3d.y;

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

