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
#include <unordered_map>
#include <set>


/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GeoIndexedRaster: public RasterObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   MAX_CACHE_SIZE     =  20;
        static const int   MAX_READER_THREADS = 200;

        static const int   MAX_FINDER_THREADS = 16;
        static const int   MIN_FEATURES_PER_FINDER_THREAD = 20;

        static const char* FLAGS_TAG;
        static const char* VALUE_TAG;
        static const char* DATE_TAG;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            OGRPoint                    point;
            int64_t                     index;
        } ogr_point_info_t;

        typedef struct {
            ogr_point_info_t            pointInfo;
            RasterSample*               sample;
            uint32_t                    ssErrors;
        } point_sample_t;

        struct unique_raster_t;
        typedef struct RasterInfo {
            bool                       dataIsElevation;
            std::string                tag;
            std::string                fileName;
            unique_raster_t*           uraster;  // only used for batch reading

            RasterInfo(void): dataIsElevation(false), uraster(NULL) {}
        } raster_info_t;

        typedef struct unique_raster_t {
            raster_info_t               rinfo;
            double                      gpsTime;
            uint64_t                    fileId;
            std::vector<point_sample_t> pointSamples;
        } unique_raster_t;


        typedef struct RaserGroup {
            std::string                id;
            std::vector<raster_info_t> infovect;
            TimeLib::gmt_time_t        gmtDate;
            int64_t                    gpsTime;

            RaserGroup(void): gmtDate{0,0,0,0,0,0}, gpsTime(0) {}
        } rasters_group_t;

        typedef Ordering<rasters_group_t*, unsigned long> GroupOrdering;

        typedef struct BatchReader {
            GeoIndexedRaster*   obj;
            unique_raster_t*    uraster;
            Thread*             thread;
            Cond                sync;
            bool                run;
            explicit BatchReader(GeoIndexedRaster* _obj);
            ~BatchReader(void);
        } batch_reader_t;


        typedef struct CacheItem {
            bool            enabled;
            RasterSample*   sample;
            RasterSubset*   subset;
            GdalRaster*     raster;
            ~CacheItem(void) {delete raster;}
        } cacheitem_t;

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
        typedef struct Finder {
            GeoIndexedRaster*             obj;
            OGRGeometry*                  geo;
            range_t                       range;
            std::vector<OGRFeature*>*     featuresList;
            std::vector<rasters_group_t*> rasterGroups;
            Thread*                       thread;
            Cond                          sync;
            bool                          run;
            bool                          fake;
            explicit Finder(GeoIndexedRaster* _obj, std::vector<OGRFeature*>* _featuresList, bool _fake=false);
                    ~Finder(void);
        } finder_t;


        typedef struct {
            double        points2polyTime;
            double        unioningTime;
        } union_maker_stats_t;

        typedef struct UnionMaker {
            GeoIndexedRaster*                obj;
            range_t                          pointsRange;
            const std::vector<point_info_t>* points;
            OGRGeometry*                     unionPolygon;
            union_maker_stats_t              stats;

            explicit UnionMaker (GeoIndexedRaster* _obj, const std::vector<point_info_t>* _points);
        } union_maker_t;

        typedef struct {
            ogr_point_info_t   pointInfo;
            GroupOrdering*     groupList;
        } point_groups_t;

        /* Typedef for the global map (raster file name -> set of unique point IDs) */
        typedef std::unordered_map<std::string, std::set<uint32_t>> raster_points_map_t;

        typedef struct RastersGroupsFinder {
            GeoIndexedRaster*                obj;
            range_t                          pointsRange;
            const std::vector<point_info_t>* points;
            std::vector<point_groups_t>      pointsGroups;
            raster_points_map_t              rasterToPointsMap;

            explicit RastersGroupsFinder (GeoIndexedRaster* _obj, const std::vector<point_info_t>* _points);
        } rasters_groups_finder_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     init              (void);
        static void     deinit            (void);
        uint32_t        getSamples        (const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param=NULL) final;
        uint32_t        getSamples        (const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param=NULL) final;
        uint32_t        getSubsets        (const MathLib::extent_t&  extent, int64_t gps, List<RasterSubset*>& slist, void* param=NULL) final;
                       ~GeoIndexedRaster  (void) override;

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef Dictionary<cacheitem_t*> CacheDictionary;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                         GeoIndexedRaster      (lua_State* L, GeoParms* _parms, GdalRaster::overrideCRS_t cb=NULL);
        virtual uint32_t getBatchGroupSamples  (const rasters_group_t* rgroup, List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx);
        static  uint32_t getBatchGroupFlags    (const rasters_group_t* rgroup, uint32_t pointIndx);

        virtual void     getGroupSamples       (const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags);
        virtual void     getGroupSubsets       (const rasters_group_t* rgroup, List<RasterSubset*>& slist);
        uint32_t         getGroupFlags         (const rasters_group_t* rgroup);

        static double    getGmtDate            (const OGRFeature* feature, const char* field,  TimeLib::gmt_time_t& gmtDate);
        bool             openGeoIndex          (const OGRGeometry* geo, const std::vector<point_info_t>* points);
        virtual bool     getFeatureDate        (const OGRFeature* feature, TimeLib::gmt_time_t& gmtDate);
        virtual void     getIndexFile          (const OGRGeometry* geo, std::string& file, const std::vector<point_info_t>* points=NULL) = 0;
        virtual bool     findRasters           (finder_t* finder) = 0;
        void             sampleRasters         (OGRGeometry* geo);
        bool             sample                (OGRGeometry* geo, int64_t gps, GroupOrdering* groupList);
        void             emptyFeaturesList     (void);
        static OGRGeometry* getConvexHull      (const std::vector<point_info_t>* points);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        CacheDictionary          cache;
        std::vector<OGRFeature*> featuresList;
        OGRPolygon               geoIndexPoly;
        uint32_t                 ssErrors;

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            double        findRastersTime;
            double        findUniqueRastersTime;
            double        findPointsForUniqueRastersTime;
            double        getSamplesTime;
            double        popluateSamplesListTime;
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

        std::vector<range_t>      findersRanges;
        List<finder_t*>           finders;
        List<reader_t*>           readers;
        List<batch_reader_t*>     batchReaders;
        GdalRaster::overrideCRS_t crscb;

        std::string               indexFile;
        GdalRaster::bbox_t        bbox;
        uint32_t                  rows;
        uint32_t                  cols;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaDimensions       (lua_State* L);
        static int      luaBoundingBox      (lua_State* L);
        static int      luaCellSize         (lua_State* L);

        static void*    finderThread        (void *param);
        static void*    readerThread        (void *param);
        static void*    batchReaderThread   (void *param);
        static void*    unionThread         (void* param);
        static void*    groupsFinderThread  (void *param);

        bool            createFinderThreads (void);
        bool            createReaderThreads (uint32_t  rasters2sample);
        bool            createBatchReaderThreads(uint32_t rasters2sample);

        bool            updateCache         (uint32_t& rasters2sample, const GroupOrdering* groupList);
        bool            filterRasters       (int64_t gps, GroupOrdering* groupList);
        bool            findRastersParallel (OGRGeometry* geo, GroupOrdering* groupList, const std::vector<point_info_t>* points=NULL);
        OGRGeometry*    getBufferedPoints   (const std::vector<point_info_t>* points);
};

#endif  /* __geo_indexed_raster__ */
