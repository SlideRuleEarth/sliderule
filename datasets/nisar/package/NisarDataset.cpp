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

#include "NisarDataset.h"
#include "GdalRaster.h"
#include <regex>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* NisarDataset::URL_str = "https://sds-n-cumulus-prod-nisar-sample-data.s3.us-west-2.amazonaws.com";

/* Valid L2 GOFF bands */
const char* NisarDataset::validL2GOFFbands[] = {"//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer1/alongTrackOffset",
                                                "//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer2/alongTrackOffset",
                                                "//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer3/alongTrackOffset",
                                                "//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer1/slantRangeOffset",
                                                "//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer2/slantRangeOffset",
                                                "//science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer3/slantRangeOffset"};

#define validL2GOFFbandsCnt (sizeof (validL2GOFFbands) / sizeof (const char *))

Mutex NisarDataset::transfMutex;
Mutex NisarDataset::crsMutex;
std::unordered_map<std::string, std::array<double, 6>> NisarDataset::transformCache;
std::unordered_map<std::string, int>                   NisarDataset::crsCache;

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
NisarDataset::NisarDataset(lua_State* L, RequestFields* rqst_parms, const char* key):
 GeoIndexedRaster(L, rqst_parms, key, overrideGeoTransform, overrideTargetCRS),
 filePath(parms->asset.asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(),
                                        const_cast<GByte*>(reinterpret_cast<const GByte*>(parms->catalog.value.c_str())), // source bytes
                                        static_cast<vsi_l_offset>(parms->catalog.value.size()), // length in bytes
                                        FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
NisarDataset::~NisarDataset(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void NisarDataset::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    static_cast<void>(geo);
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void NisarDataset::getIndexFile(const std::vector<point_info_t>* points, std::string& file)
{
    static_cast<void>(points);
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool NisarDataset::findRasters(raster_finder_t* finder)
{
    const std::vector<OGRFeature*>* flist = finder->featuresList;
    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = 0;
    const uint32_t end     = flist->size();

    try
    {
        /* Search through feature list */
        for(uint32_t i = start; i < end; i++)
        {
            OGRFeature* feature = flist->at(i);
            OGRGeometry *rastergeo = feature->GetGeometryRef();

            if (!rastergeo->Intersects(geo)) continue;

            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->gpsTime = getGmtDate(feature, DATE_TAG, rgroup->gmtDate) / 1000.0;

            const char* fname = feature->GetFieldAsString("url");
            if(fname && strlen(fname) > 0)
            {
                const std::string fileName(fname);
                const size_t pos = strlen(URL_str);
                const std::string hdf5file = filePath + fileName.substr(pos);

                /* Build two data sets: along track and slant range offsets */
                for(uint32_t j = 0; j < validL2GOFFbandsCnt; j++)
                {
                    raster_info_t rinfo;
                    rinfo.elevationBandNum   = 0;
                    rinfo.tag                = VALUE_TAG;
                    const std::string dsName = "HDF5:\"" + hdf5file + "\":" + validL2GOFFbands[j];
                    rinfo.fileId             = finder->fileDict.add(dsName);
                    rgroup->infovect.push_back(rinfo);
                }
            }
            // mlog(DEBUG, "Added group with %ld rasters", rgroup->infovect.size());
            finder->rasterGroups.push_back(rgroup);
        }
        // mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/*----------------------------------------------------------------------------
 * getSerialGroupSamples
 *----------------------------------------------------------------------------*/
void NisarDataset::getSerialGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags)
{
    //TODO: for L2 GEOFF we will be processing all 3 layers of datasets, for now return them all
    for(const auto& rinfo : rgroup->infovect)
    {
        const char* key = fileDictGet(rinfo.fileId);
        cacheitem_t* item;
        if(cache.find(key, &item) && !item->bandSample.empty())
        {
            RasterSample* sample = new RasterSample(*item->bandSample[0]);

            /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
            if(sample == NULL) continue;

            sample->flags = flags;
            slist.add(sample);
        }
    }
}

/*----------------------------------------------------------------------------
 * getSerialGroupSamples
 *----------------------------------------------------------------------------*/
uint32_t NisarDataset::getBatchGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx)
{
    // TODO: implement
    static_cast<void>(rgroup);
    static_cast<void>(slist);
    static_cast<void>(flags);
    static_cast<void>(pointIndx);

    return SS_NO_ERRORS;
}

/*----------------------------------------------------------------------------
 * overrideGeoTransform
 *----------------------------------------------------------------------------*/
CPLErr NisarDataset::overrideGeoTransform(double* gtf, const void* param)
{
    const char* datasetPath = static_cast<const char*>(param);
#if 0
    gtf[0] =   365520.0;
    gtf[1] =       80.0;
    gtf[2] =        0.0;
    gtf[3] = 03913580.0;
    gtf[4] =        0.0;
    gtf[5] =      -80.0;
#else
    const std::regex pattern(R"(HDF5:\"([^\"]+)\":(.*))");
    std::cmatch match;
    if (!std::regex_match(datasetPath, match, pattern) || match.size() < 3)
        return CE_Failure;

    const std::string hdf5File = match[1];
    const std::string datasetSubpath = match[2];

    /* Strip trailing dataset name (e.g., /alongTrackOffset, /slantRangeOffset) */
    const size_t lastSlash = datasetSubpath.find_last_of('/');
    if (lastSlash == std::string::npos)
        return CE_Failure;

    std::string layerPrefix = datasetSubpath.substr(0, lastSlash);

    /* All three layers uset the same grids, for performance always use the first layer */
    if (!layerPrefix.empty())
    {
        const char lastChar = layerPrefix.back();
        if (lastChar == '2' || lastChar == '3')
            layerPrefix.back() = '1';
    }

    /* Build a cache key unique per file+layer*/
    const std::string cacheKey = hdf5File + "|" + layerPrefix;

    /* Check cache first */
    transfMutex.lock();
    {
        auto it = transformCache.find(cacheKey);
        if (it != transformCache.end())
        {
            std::copy(it->second.begin(), it->second.end(), gtf);
            transfMutex.unlock();
            return CE_None;
        }
    }
    transfMutex.unlock();

    /* Not cached, compute geoTransform */

    const std::string xPath = "HDF5:\"" + hdf5File + "\":" + layerPrefix + "/xCoordinates";
    const std::string yPath = "HDF5:\"" + hdf5File + "\":" + layerPrefix + "/yCoordinates";

    GDALDataset* xDset = static_cast<GDALDataset*>(GDALOpen(xPath.c_str(), GA_ReadOnly));
    if (!xDset)
    {
        return CE_Failure;
    }
    GDALRasterBand* xBand = xDset->GetRasterBand(1);
    if (!xBand)
    {
        GDALClose(xDset);
        return CE_Failure;
    }

    GDALDataset* yDset = static_cast<GDALDataset*>(GDALOpen(yPath.c_str(), GA_ReadOnly));
    if (!yDset)
    {
        GDALClose(xDset);
        return CE_Failure;
    }
    GDALRasterBand* yBand = yDset->GetRasterBand(1);
    if (!yBand)
    {
        GDALClose(xDset);
        GDALClose(yDset);
        return CE_Failure;
    }

    double xVals[2], yVals[2];
    if (xBand->RasterIO(GF_Read, 0, 0, 2, 1, &xVals[0], 2, 1, GDT_Float64, 0, 0) != CE_None ||
        yBand->RasterIO(GF_Read, 0, 0, 2, 1, &yVals[0], 2, 1, GDT_Float64, 0, 0) != CE_None)
    {
        GDALClose(xDset);
        GDALClose(yDset);
        return CE_Failure;
    }

    /* Set GeoTransform */
    gtf[0] = xVals[0];
    gtf[1] = xVals[1] - xVals[0];
    gtf[2] = 0.0;
    gtf[3] = yVals[0];
    gtf[4] = 0.0;
    gtf[5] = yVals[1] - yVals[0];

    GDALClose(xDset);
    GDALClose(yDset);

    /* Add to cache, make sure another thread did not do it already */
    transfMutex.lock();
    {
        auto it = transformCache.find(cacheKey);
        if (it == transformCache.end())
            transformCache[layerPrefix] = {gtf[0], gtf[1], gtf[2], gtf[3], gtf[4], gtf[5]};
    }
    transfMutex.unlock();
#endif

    return CE_None;
}

/*----------------------------------------------------------------------------
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
OGRErr NisarDataset::overrideTargetCRS(OGRSpatialReference& target, const void* param)
{
    const char* datasetPath = static_cast<const char*>(param);

#if 0
    target.importFromEPSG(32611);
#else
    const std::regex pattern(R"(HDF5:\"([^\"]+)\":(.*))");
    std::cmatch match;
    if (!std::regex_match(datasetPath, match, pattern) || match.size() < 3)
        return OGRERR_FAILURE;

    const std::string hdf5File = match[1];
    const std::string datasetSubpath = match[2];

    /* Extract pixelOffsets group prefix (e.g., /science/LSAR/GOFF/grids/frequencyA/pixelOffsets) */
    const std::string anchor = "/pixelOffsets";
    const size_t pos = datasetSubpath.find(anchor);
    if (pos == std::string::npos)
        return OGRERR_FAILURE;

    /* All three layers use the same projection and grids */
    const std::string pixelOffsetGroup = datasetSubpath.substr(0, pos + anchor.size());

    /* Build a cache key unique per file+pixelOffsets group */
    const std::string cacheKey = hdf5File + "|" + pixelOffsetGroup;

    /* Check cache first */
    crsMutex.lock();
    {
        auto it = crsCache.find(cacheKey);
        if (it != crsCache.end())
        {
            const OGRErr err = target.importFromEPSG(it->second);
            crsMutex.unlock();
            return err;
        }
    }
    crsMutex.unlock();

    /* Not cached, get CRS from root HDF5 file */

    GDALDataset* dset = static_cast<GDALDataset*>(GDALOpen(hdf5File.c_str(), GA_ReadOnly));
    if (!dset)
        return OGRERR_FAILURE;

    const std::string epsgKey = "science_LSAR_GOFF_grids_frequencyA_pixelOffsets_projection_epsg_code";
    const char* epsgStr = dset->GetMetadataItem(epsgKey.c_str());
    if (epsgStr == NULL)
    {
        GDALClose(dset);
        return OGRERR_FAILURE;
    }

    const int epsg = std::atoi(epsgStr);
    if(target.importFromEPSG(epsg) != OGRERR_NONE)
    {
        GDALClose(dset);
        return OGRERR_FAILURE;
    }

    /* Store in cache, make sure another thread did not do it already */
    crsMutex.lock();
    {
        auto it = crsCache.find(cacheKey);
        if (it == crsCache.end())
            crsCache[pixelOffsetGroup] = epsg;
    }
    crsMutex.unlock();

    GDALClose(dset);
#endif
    return OGRERR_NONE;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/
