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

#include "PgcDemStripsRaster.h"

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

static const std::vector<const char*> dates = {"start_datetime", "end_datetime"};

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::PgcDemStripsRaster(lua_State *L, GeoParms* _parms, const char* dem_name, const char* geo_suffix, GdalRaster::overrideCRS_t cb):
    GeoIndexedRaster(L, _parms, cb),
    demName(dem_name),
    path2geocells(_parms->asset->getPath() + std::string(geo_suffix))
{
    const std::size_t pos = path2geocells.find(demName);
    if (pos == std::string::npos)
        throw RunTimeException(DEBUG, RTE_ERROR, "Invalid path to geocells: %s", path2geocells.c_str());

    filePath = path2geocells.substr(0, pos);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PgcDemStripsRaster::~PgcDemStripsRaster(void) = default;


/*----------------------------------------------------------------------------
 * getFeatureDate
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::getFeatureDate(const OGRFeature* feature, TimeLib::gmt_time_t& gmtDate)
{
    double gps = 0;

    for(const auto& s : dates)
    {
        TimeLib::gmt_time_t gmt;
        gps += getGmtDate(feature, s, gmt);
    }
    gps = gps / dates.size();
    gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));

    return true;
}


/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::openGeoIndex(const OGRGeometry* geo)
{
    /* For point call parent class */
    if(GdalRaster::ispoint(geo))
        return GeoIndexedRaster::openGeoIndex(geo);

    /*
     * Create a list of minx, miny  1° x 1° geocell points contained in AOI
     * For each point get geojson file
     * Open file, get list of features.
     */
    const OGRPolygon* poly = geo->toPolygon();
    OGREnvelope env;
    poly->getEnvelope(&env);

    const double minx = floor(env.MinX);
    const double miny = floor(env.MinY);
    const double maxx = ceil(env.MaxX);
    const double maxy = ceil(env.MaxY);

    /* Create poly geometry for all index files */
    geoIndexPoly = GdalRaster::makeRectangle(minx, miny, maxx, maxy);

    emptyFeaturesList();

    for(long ix = minx; ix < maxx; ix++ )
    {
        for(long iy = miny; iy < maxy; iy++)
        {
            std::string newFile;
            _getIndexFile(ix, iy, newFile);

            GDALDataset* dset = NULL;
            try
            {
                /* Open new vector data set*/
                dset = static_cast<GDALDataset*>(GDALOpenEx(newFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL));
                if(dset == NULL)
                {
                    /* If index file for this ix, iy does not exist, continue */
                    mlog(DEBUG, "Failed to open geojson index file: %s:", newFile.c_str());
                    continue;
                }

                OGRLayer* layer = dset->GetLayer(0);
                CHECKPTR(layer);

                /*
                 * Clone all features and store them for performance/speed of feature lookup
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

                mlog(DEBUG, "Loaded %lu index file features/rasters from: %s", featuresList.size(), newFile.c_str());
                GDALClose((GDALDatasetH)dset);
            }
            catch(const RunTimeException& e)
            {
                /* If geocell does not have a geojson index file, ignore it and don't count it as error */
                if(dset) GDALClose((GDALDatasetH)dset);
            }
        }
    }

    if(featuresList.empty())
    {
        /* All geocells were 'empty' */
        geoIndexPoly.empty();
        ssError |= SS_INDEX_FILE_ERROR;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    if(GdalRaster::ispoint(geo))
    {
        const OGRPoint* poi = geo->toPoint();
        _getIndexFile(poi->getX(), poi->getY(), file);
    }
}

/*----------------------------------------------------------------------------
 * getMaxBatchThreads
 *----------------------------------------------------------------------------*/
uint32_t PgcDemStripsRaster::getMaxBatchThreads(void)
{
    /*
     * The average number of strips for a point is between 10 to 20.
     * There are areas where the number of strips can be over 100.
     * Limit the number of batch threads to 1.
     */
    return 1;
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::findRasters(finder_t* finder)
{

    const OGRGeometry* geo    = finder->geo;
    const uint32_t start_indx = finder->range.start_indx;
    const uint32_t end_indx   = finder->range.end_indx;

    /*
     * Find rasters and their dates.
     * geojson index file contains two dates: 'start_date' and 'end_date'
     * Calculate the rster date as mid point between start and end dates.
     *
     * The file name/path contains a date in it.
     * We cannot use it because it is the date of the earliest image of the stereo pair.
     * For intrack pairs (pairs collected intended for stereo) the two images are acquired within a few minutes of each other.
     * For cross-track images (opportunistic stereo pairs made from mono collects)
     * the two images can be up to 30 days apart.
     *
     */
    try
    {
        for(uint32_t i = start_indx; i < end_indx; i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry* rastergeo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if (!rastergeo->Intersects(geo)) continue;

            /* geojson index files hosted by PGC only contain listing of dems
             * In order to read quality mask raster for each strip we need to build a path to it.
             */
            const char *fname = feature->GetFieldAsString("Dem");
            if(fname && strlen(fname) > 0)
            {
                std::string fileName(fname);
                std::size_t pos = fileName.find(demName);
                if (pos == std::string::npos)
                    throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", demName.c_str());

                fileName = filePath + fileName.substr(pos);

                rasters_group_t* rgroup = new rasters_group_t;
                raster_info_t demRinfo;
                demRinfo.dataIsElevation = true;
                demRinfo.tag = VALUE_TAG;
                demRinfo.fileName = fileName;
                demRinfo.rasterGeo = rastergeo->clone();

                /* bitmask raster, ie flags_file */
                if(parms->flags_file)
                {
                    const std::string endToken    = "_dem.tif";
                    const std::string newEndToken = "_bitmask.tif";
                    pos = fileName.rfind(endToken);
                    if(pos != std::string::npos)
                    {
                        fileName.replace(pos, endToken.length(), newEndToken.c_str());
                    }
                    else fileName.clear();

                    raster_info_t flagsRinfo;
                    flagsRinfo.dataIsElevation = false;
                    flagsRinfo.tag = FLAGS_TAG;
                    flagsRinfo.fileName = fileName;
                    flagsRinfo.rasterGeo = rastergeo->clone();  /* Should be the same as data raster */

                    if(!flagsRinfo.fileName.empty())
                    {
                        rgroup->infovect.push_back(flagsRinfo);
                    }
                }

                double gps = 0;
                for(const auto& s: dates)
                {
                    TimeLib::gmt_time_t gmt;
                    gps += getGmtDate(feature, s, gmt);
                }
                gps = gps/dates.size();

                /* Set raster group time and group id */
                rgroup->gmtDate = TimeLib::gps2gmttime(static_cast<int64_t>(gps));
                rgroup->gpsTime = static_cast<int64_t>(gps);
                rgroup->infovect.push_back(demRinfo);
                finder->rasterGroups.push_back(rgroup);
            }
        }
        mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * _getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::_getIndexFile(double lon, double lat, std::string& file)
{
    /*
     * Strip DEM ﬁles are distributed in folders according to the 1° x 1° geocell in which the geometric center resides.
     * Geocell folder naming refers to the southwest degree corner coordinate
     * (e.g., folder n72e129 will contain all ArcticDEM strip ﬁles with centroids within 72° to 73° north latitude, and 129° to 130° east longitude).
     *
     * https://www.pgc.umn.edu/guides/stereo-derived-elevation-models/pgcs-dem-products-arcticdem-rema-and-earthdem/#section-9
     *
     * NOTE: valid latitude strings for Arctic DEMs are 'n59' and up. Nothing below 59. 'n' is always followed by two digits.
     *       valid latitude strings for REMA are 's54' and down. Nothing above 54. 's' is always followed by two digits.
     *       valid longitude strings are 'e/w' followed by zero padded 3 digits.
     *       example:  lat 61, lon -120.3  ->  n61w121
     *                 lat 61, lon  -50.8  ->  n61w051
     *                 lat 61, lon   -5    ->  n61w005
     *                 lat 61, lon    5    ->  n61e005
     */

    /* Round to geocell location */
    const int _lon = static_cast<int>(floor(lon));
    const int _lat = static_cast<int>(floor(lat));

    char lonBuf[32];
    char latBuf[32];

    sprintf(lonBuf, "%03d", abs(_lon));
    sprintf(latBuf, "%02d", abs(_lat));

    const std::string lonStr(lonBuf);
    const std::string latStr(latBuf);

    file = path2geocells +
           latStr +
           ((lon < 0) ? "w" : "e") +
           lonStr +
           ".geojson";

    mlog(DEBUG, "Using %s", file.c_str());
}

