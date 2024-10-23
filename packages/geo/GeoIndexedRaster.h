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

#ifndef __geo_indexed_raster__
#define __geo_indexed_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GdalRaster.h"
#include "RasterObject.h"
#include "Ordering.h"
#include "GeoRtree.h"
#include <unordered_map>
#include <set>

/* GEOS C++ API is unstable, use C API */
#include <geos_c.h>


/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GeoIndexedRaster: public RasterObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double TOLERANCE;

        static const int   MAX_CACHE_SIZE     =  20;
        static const int   MAX_READER_THREADS = 200;

        static const char* FLAGS_TAG;
        static const char* VALUE_TAG;
        static const char* DATE_TAG;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Raster Sample used by batch sampling */
        typedef struct PointSample {
            OGRPoint                    point;
            int64_t                     pointIndex;    // index to the user provided list of points to sample
            RasterSample*               sample;        // sample created by the batch reader thread
            std::atomic<bool>           sampleReturned;// multiple rasters may share the same sample,
                                                       // this flag is used to avoid returning the same sample, if set a copy of the sample is returned
            uint32_t                    ssErrors;      // sampling errors

            PointSample(const OGRPoint& _point, int64_t _pointIndex):
               point(_point), pointIndex(_pointIndex), sample(NULL), sampleReturned(false), ssErrors(SS_NO_ERRORS) {}

            PointSample(const PointSample& ps):
               point(ps.point), pointIndex(ps.pointIndex), sample(ps.sample), sampleReturned(ps.sampleReturned.load()), ssErrors(ps.ssErrors) {}

        } point_sample_t;

        struct UniqueRaster;

        /** Raster information needed for sampling */
        typedef struct RasterInfo {
            bool                    dataIsElevation;
            std::string             tag;          // "Value", "Flags", "Date"
            std::string             fileName;     // Raster file name
            UniqueRaster*           uraster;      // Pointer to the unique raster which contains the sample for this raster

            RasterInfo(void): dataIsElevation(false), uraster(NULL) {}
        } raster_info_t;

        /* Group of rasters belonging to the same geojson stac catalog feature */
        typedef struct RaserGroup {
            std::string                featureId;  // stac catalog feature id
            std::vector<raster_info_t> infovect;   // vector of rasters belonging to the same stac catalog feature
            TimeLib::gmt_time_t        gmtDate;    // feature date (can be computed from start/end dates)
            int64_t                    gpsTime;    // feature gps time

            RaserGroup(void): gmtDate{0,0,0,0,0,0}, gpsTime(0) {}
        } rasters_group_t;

        /* Raster and associated points to sample, used by batch sampling */
        typedef struct UniqueRaster {
            bool                        dataIsElevation;
            const std::string&          fileName;
            uint64_t                    fileId;         // fileDictionary id
            std::vector<point_sample_t> pointSamples;   // vector of samples for each point in this raster
            explicit UniqueRaster(bool _dataIsElevation, const std::string& _fileName):
                                 dataIsElevation(_dataIsElevation), fileName(_fileName), fileId(0) {}
        } unique_raster_t;

        typedef Ordering<rasters_group_t*, unsigned long> GroupOrdering;

        /* Batch reader thread info used by batch sampling code */
        typedef struct BatchReader {
            GeoIndexedRaster*   obj;
            unique_raster_t*    uraster;
            Thread*             thread;
            Cond                sync;
            bool                run;
            explicit BatchReader(GeoIndexedRaster* _obj);
            ~BatchReader(void);
        } batch_reader_t;

        /* Cache used by serial sampling code */
        typedef struct CacheItem {
            bool            enabled;
            RasterSample*   sample;
            RasterSubset*   subset;
            GdalRaster*     raster;
            ~CacheItem(void) {delete raster;}
        } cacheitem_t;

        /* Reader thread info used by serial sampling code */
        typedef struct Reader {
            GeoIndexedRaster*   obj;
            OGRGeometry*        geo;
            Thread*             thread;
            cacheitem_t*        entry;
            Cond                sync;
            bool                run;
            explicit Reader(GeoIndexedRaster* _obj);
            ~Reader(void);
        } reader_t;

        typedef RasterObject::range_t range_t;

        /* Point and it's asociated group list */
        typedef struct {
            OGRPoint           point;
            int64_t            pointIndex;    // index to the user provided list of points to sample
            GroupOrdering*     groupList;     // all raster groups that intersect with this point
        } point_groups_t;

        /* Samples collector thread info used by batch sampling code */
        typedef struct SampleCollector {
            GeoIndexedRaster*                  obj;
            range_t                            pGroupsRange;  // range of point groups to process for this thread
            const std::vector<point_groups_t>& pointsGroups;
            std::vector<sample_list_t*>        slvector;      // vecotor of sample lists to be returned to the user
            uint32_t                           ssErrors;      // sampling errors
            explicit SampleCollector(GeoIndexedRaster* _obj, const std::vector<point_groups_t>& _pointsGroups);
        } sample_collector_t;

        /* Map of raster file name and unique ordered points to be sampled in that raster */
        typedef std::unordered_map<std::string, std::set<uint32_t>> raster_points_map_t;

        /* GroupsFinder thread info used by batch sampling code */
        typedef struct GroupsFinder {
            GeoIndexedRaster*                obj;
            range_t                          pointsRange;
            const std::vector<point_info_t>* points;
            std::vector<point_groups_t>      pointsGroups;
            raster_points_map_t              rasterToPointsMap;

            explicit GroupsFinder (GeoIndexedRaster* _obj, const std::vector<point_info_t>* _points);
        } groups_finder_t;

        /* Used by GroupsFinder */
        typedef struct RasterFinder {
            const OGRGeometry*              geo;
            const std::vector<OGRFeature*>* featuresList;  // features to test for intersection with geo
            std::vector<rasters_group_t*>   rasterGroups;  // result raster groups which intersect with geo
            explicit RasterFinder(const OGRGeometry* geo, const std::vector<OGRFeature*>* _featuresList);
        } raster_finder_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     init              (void);
        static void     deinit            (void);
        uint32_t        getSamples        (const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param=NULL) final;
        uint32_t        getSamples        (const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param=NULL) final;
        uint32_t        getSubsets        (const MathLib::extent_t&  extent, int64_t gps, List<RasterSubset*>& slist, void* param=NULL) final;

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef Dictionary<cacheitem_t*> CacheDictionary;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                         GeoIndexedRaster      (lua_State* L, RequestFields* _parms, const char* key, GdalRaster::overrideCRS_t cb=NULL);
        virtual uint32_t getBatchGroupSamples  (const rasters_group_t* rgroup, List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx);
        static  uint32_t getBatchGroupFlags    (const rasters_group_t* rgroup, uint32_t pointIndx);

        virtual void     getGroupSamples       (const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags);
        virtual void     getGroupSubsets       (const rasters_group_t* rgroup, List<RasterSubset*>& slist);
        uint32_t         getGroupFlags         (const rasters_group_t* rgroup);

        static double    getGmtDate            (const OGRFeature* feature, const char* field,  TimeLib::gmt_time_t& gmtDate);
        bool             openGeoIndex          (const OGRGeometry* geo, const std::vector<point_info_t>* points);
        virtual bool     getFeatureDate        (const OGRFeature* feature, TimeLib::gmt_time_t& gmtDate);
        virtual void     getIndexFile          (const OGRGeometry* geo, std::string& file, const std::vector<point_info_t>* points=NULL) = 0;
        virtual bool     findRasters           (raster_finder_t* finder) = 0;
        void             sampleRasters         (OGRGeometry* geo);
        bool             sample                (OGRGeometry* geo, int64_t gps, GroupOrdering* groupList);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        CacheDictionary          cache;     // Used by serial sampling
        uint32_t                 ssErrors;

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct PerfStats {
            double  spatialFilterTime;
            double  findRastersTime;
            double  findUniqueRastersTime;
            double  samplesTime;
            double  collectSamplesTime;

            PerfStats (void) : spatialFilterTime(0), findRastersTime(0), findUniqueRastersTime(0), samplesTime(0), collectSamplesTime(0) {}
            void clear(void) { spatialFilterTime = 0; findRastersTime = 0; findUniqueRastersTime = 0; samplesTime = 0; collectSamplesTime = 0; }
            void log  (event_level_t lvl)
            {
                mlog(lvl, "Performance Stats:");
                mlog(lvl, "spatialFilter: %12.3lf", spatialFilterTime);
                mlog(lvl, "findingRasters:%12.3lf", findRastersTime);
                mlog(lvl, "findingUnique: %12.3lf", findUniqueRastersTime);
                mlog(lvl, "sampling:      %12.3lf", samplesTime);
                mlog(lvl, "collecSamples: %12.3lf", collectSamplesTime);
            }
        } perf_stats_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DATA_TO_SAMPLE   = 0;
        static const int DATA_SAMPLED     = 1;
        static const int NUM_SYNC_SIGNALS = 2;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<reader_t*>           readers;
        List<batch_reader_t*>     batchReaders;
        perf_stats_t              perfStats;
        GdalRaster::overrideCRS_t crscb;

        std::string               indexFile;
        GdalRaster::bbox_t        bbox;
        uint32_t                  rows;
        uint32_t                  cols;

        GeoRtree                  geoRtree;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaDimensions       (lua_State* L);
        static int      luaBoundingBox      (lua_State* L);
        static int      luaCellSize         (lua_State* L);

        static void*    readerThread        (void *param);
        static void*    batchReaderThread   (void *param);

        static void*    groupsFinderThread  (void *param);
        static void*    samplesCollectThread(void *param);

        bool            createReaderThreads (uint32_t  rasters2sample);
        bool            createBatchReaderThreads(uint32_t rasters2sample);

        bool            updateCache         (uint32_t& rasters2sample, const GroupOrdering* groupList);
        bool            filterRasters       (int64_t gps, GroupOrdering* groupList);
        static OGRGeometry* getConvexHull   (const std::vector<point_info_t>* points);
        void            applySpatialFilter  (OGRLayer* layer, const std::vector<point_info_t>* points);

        bool            findAllGroups       (const std::vector<point_info_t>* points,
                                             std::vector<point_groups_t>& pointsGroups,
                                             raster_points_map_t& rasterToPointsMap);

        bool            findUniqueRasters   (std::vector<unique_raster_t*>& uniqueRasters,
                                             const std::vector<point_groups_t>& pointsGroups,
                                             raster_points_map_t& rasterToPointsMap);

        bool            sampleUniqueRasters (const std::vector<unique_raster_t*>& uniqueRasters);

        bool            collectSamples      (const std::vector<point_groups_t>& pointsGroups,
                                             List<sample_list_t*>& sllist);

};

#endif  /* __geo_indexed_raster__ */
