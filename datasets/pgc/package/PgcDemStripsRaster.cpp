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
PgcDemStripsRaster::~PgcDemStripsRaster(void)
{
    /* Remove combined geojson file */
    if(!combinedGeoJSON.empty())
    {
        VSIUnlink(combinedGeoJSON.c_str());
    }
}


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
 * getIndexFile
 *----------------------------------------------------------------------------*/
void PgcDemStripsRaster::getIndexFile(const OGRGeometry* geo, std::string& file, const std::vector<point_info_t>* points)
{
    const OGRPolygon* poly = NULL;
    OGRGeometry* convexHull = NULL;

    if(geo == NULL && points == NULL)
    {
        mlog(ERROR, "Both geo and points are NULL");
        ssErrors |= SS_INDEX_FILE_ERROR;
        return;
    }

    /* Determine if we have a point */
    if(geo && GdalRaster::ispoint(geo))
    {
        /* Only one index file for a point from one of the geocells */
        const OGRPoint* poi = geo->toPoint();
        _getIndexFile(poi->getX(), poi->getY(), file);
        return;
    }

    /*
     * Below we are dealing with polygons only, either from geo or created from points.
     */

    /* Determine if we have a polygon */
    if(geo && GdalRaster::ispoly(geo))
    {
        poly = geo->toPolygon();
    }

    /* Create convex hull from points */
    if(points != NULL)
    {
        convexHull = getConvexHull(points);
        if(convexHull == NULL)
        {
            mlog(ERROR, "Error creating convex hull");
            ssErrors |= SS_INDEX_FILE_ERROR;
            return;
        }
    }

    /* If we don't have a polygon and we don't have a convex hull, we have an error */
    if(poly == NULL && convexHull == NULL)
    {
        mlog(ERROR, "Invalid geometry");
        ssErrors |= SS_INDEX_FILE_ERROR;
        return;
    }

    /* We don't have a polygon, but we have a convex hull */
    if(poly == NULL && convexHull != NULL)
    {
        poly = convexHull->toPolygon();
    }

    /*
     * Find all geojson files that intersect the polygon
     */

    OGREnvelope env;
    poly->getEnvelope(&env);

    const double minx = floor(env.MinX);
    const double miny = floor(env.MinY);
    const double maxx = ceil(env.MaxX);
    const double maxy = ceil(env.MaxY);

    /* Create polygon geometry for all index files for 1° x 1° geocell grid */
    geoIndexPoly = GdalRaster::makeRectangle(minx, miny, maxx, maxy);

    /* Create a vector holding all geojson files from all geocells */
    std::vector<std::string> files;

    for(long ix = minx; ix < maxx; ix++ )
    {
        for(long iy = miny; iy < maxy; iy++)
        {
            std::string newFile;
            _getIndexFile(ix, iy, newFile);
            if(!newFile.empty())
            {
                files.push_back(newFile);
            }
        }
    }

    /* Check for geojson files found in geocells */
    if(files.empty())
    {
        mlog(ERROR, "No geojson files found");
        ssErrors |= SS_INDEX_FILE_ERROR;
        OGRGeometryFactory::destroyGeometry(convexHull);
        return;
    }

    /* If we have only one file and no convex hull to use as spatial filter, we are done */
    if(convexHull == NULL && files.size() == 1)
    {
        file = files[0];
        OGRGeometryFactory::destroyGeometry(convexHull);
        return;
    }

    /* Combine all geojson files into a single file stored in vsimem */
    if(!combinedGeoJSON.empty())
    {
        /* Remove previous combined geojson file */
        VSIUnlink(combinedGeoJSON.c_str());
    }

    combinedGeoJSON = "/vsimem/" + GdalRaster::getUUID() + "_combined.geojson";
    if(combineGeoJSONFiles(convexHull, files))
    {
        /* Set the combined geojson file as the index file */
        file = combinedGeoJSON;
    }

    /* Cleanup convex hull */
    OGRGeometryFactory::destroyGeometry(convexHull);
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::findRasters(finder_t* finder)
{

    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = finder->range.start;
    const uint32_t end     = finder->range.end;

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
        for(uint32_t i = start; i < end; i++)
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


/*----------------------------------------------------------------------------
 * combineGeoJSONFiles
 *----------------------------------------------------------------------------*/
bool PgcDemStripsRaster::combineGeoJSONFiles(OGRGeometry* convexHull, const std::vector<std::string>& inputFiles)
{
    /* Create an in-memory data source for the output */
    GDALDriver* memDriver   = GetGDALDriverManager()->GetDriverByName("Memory");
    GDALDataset* memDataset = memDriver->Create("memory", 0, 0, 0, GDT_Unknown, NULL);

    if (memDataset == NULL)
    {
        mlog(ERROR, "Failed to create in-memory dataset.");
        return false;
    }

    OGRLayer* combinedLayer = NULL;

    for (const auto& infile : inputFiles)
    {
        GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpenEx(infile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));
        if (inputDataset == NULL)
        {
            mlog(DEBUG, "Failed to open input file: %s", infile.c_str());
            continue;
        }

        /* Assuming that each file has a single layer */
        OGRLayer* inputLayer = inputDataset->GetLayer(0);
        if (inputLayer == NULL)
        {
            mlog(ERROR, "No layer found in file: %s", infile.c_str());
            GDALClose(inputDataset);
            continue;
        }

        /* Set spatial filter to the convex hull */
        if(convexHull != NULL)
        {
            inputLayer->SetSpatialFilter(convexHull);
        }

        /* If this is the first file, create the combined layer in the memory dataset */
        if (combinedLayer == NULL)
        {
            combinedLayer = memDataset->CreateLayer(inputLayer->GetName(), inputLayer->GetSpatialRef(), inputLayer->GetGeomType(), NULL);
            if (combinedLayer == NULL)
            {
                mlog(ERROR, "Failed to create combined layer in memory dataset.");
                GDALClose(inputDataset);
                GDALClose(memDataset);
                return false;
            }

            /* Copy the fields from the input layer to the combined layer */
            OGRFeatureDefn* inputFeatureDefn = inputLayer->GetLayerDefn();
            for (int i = 0; i < inputFeatureDefn->GetFieldCount(); i++)
                combinedLayer->CreateField(inputFeatureDefn->GetFieldDefn(i));
        }

        /* Copy features from the input layer to the combined layer */
        inputLayer->ResetReading();
        OGRFeature* inputFeature = NULL;
        while ((inputFeature = inputLayer->GetNextFeature()) != NULL)
        {
            OGRErr err = OGRERR_NONE;
            OGRFeature* combinedFeature = OGRFeature::CreateFeature(combinedLayer->GetLayerDefn());
            err |= combinedFeature->SetFrom(inputFeature);
            err |= combinedLayer->CreateFeature(combinedFeature);
            if(err != OGRERR_NONE)
            {
                mlog(ERROR, "Failed to copy feature from input layer to combined layer.");
                OGRFeature::DestroyFeature(inputFeature);
                OGRFeature::DestroyFeature(combinedFeature);
                GDALClose(inputDataset);
                GDALClose(memDataset);
                return false;
            }
            OGRFeature::DestroyFeature(inputFeature);
            OGRFeature::DestroyFeature(combinedFeature);
        }

        GDALClose(inputDataset);
    }

    /* Write the combined layer to a GeoJSON file in the /vsimem filesystem */
    GDALDriver* jsonDriver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    if (jsonDriver == NULL)
    {
        mlog(ERROR, "GeoJSON driver not available.");
        GDALClose(memDataset);
        return false;
    }

    GDALDataset* vsiDataset = jsonDriver->CreateCopy(combinedGeoJSON.c_str(), memDataset, FALSE, NULL, NULL, NULL);
    if (vsiDataset == NULL)
    {
        mlog(ERROR, "Failed to create GeoJSON in /vsimem.");
    }
    else
    {
        mlog(INFO, "GeoJSON successfully created in /vsimem at %s", combinedGeoJSON.c_str());
    }

    /* Cleanup */
    GDALClose(vsiDataset);
    GDALClose(memDataset);

    return true;
}
