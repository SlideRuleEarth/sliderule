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

#include "LandsatHlsRaster.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/* Landsat 8 */
const char* LandsatHlsRaster::L8_bands[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B09", "B10", "B11",
                                           "SAA", "SZA", "VAA", "VZA", "Fmask"};
/* Sentinel 2 */
const char* LandsatHlsRaster::S2_bands[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B08", "B09", "B10",
                                           "B11", "B12", "B8A", "SAA", "SZA",
                                           "VAA", "VZA", "Fmask"};
/* Algorithm names (not real bands) */
const char* LandsatHlsRaster::ALGO_names[] = {"NDSI", "NDVI", "NDWI"};

/* Algorithm bands for L8 and S2 combined */
const char* LandsatHlsRaster::ALGO_bands[] = {"B03", "B04", "B05", "B06", "B8A", "B11"};


#define L8_bandCnt   (sizeof (L8_bands)   / sizeof (const char *))
#define S2_bandCnt   (sizeof (S2_bands)   / sizeof (const char *))
#define ALGO_nameCnt (sizeof (ALGO_names) / sizeof (const char *))
#define ALGO_bandCnt (sizeof (ALGO_bands) / sizeof (const char *))

#define MAX_LANDSAT_RASTER_GROUP_SIZE (std::max(S2_bandCnt, L8_bandCnt) + ALGO_nameCnt)

const char* LandsatHlsRaster::URL_str = "https://data.lpdaac.earthdatacloud.nasa.gov/lp-prod-protected";

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LandsatHlsRaster::LandsatHlsRaster(lua_State *L, RequestFields* rqst_parms, const char* key):
 GeoIndexedRaster(L, rqst_parms, key),
 filePath(parms->asset.asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    ndsi = ndvi = ndwi = false;

    if(!validateBandNames())
    {
        VSIUnlink(indexFile.c_str());
        throw RunTimeException(DEBUG, RTE_ERROR, "Invalid band names specified");
    }


    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(),
                                        const_cast<GByte*>(reinterpret_cast<const GByte*>(parms->catalog.value.c_str())), // source bytes
                                        static_cast<vsi_l_offset>(parms->catalog.value.size()), // length in bytes
                                        FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);

    /* Create dictionary of bands and algo names to process */
    for(int i = 0; i < parms->bands.length(); i++)
    {
        const string& name = parms->bands[i];

        /* Add band to dictionary of bands but don't override if already there */
        auto it = bandsDict.find(name);
        if(it == bandsDict.end())
            bandsDict[name] = true;

        if(strcasecmp(name.c_str(), "NDSI") == 0) ndsi = true;
        if(strcasecmp(name.c_str(), "NDVI") == 0) ndvi = true;
        if(strcasecmp(name.c_str(), "NDWI") == 0) ndwi = true;
    }

    /* If user specified algortithm(s), add needed bands to dictionary of bands */
    if(ndsi || ndvi || ndwi)
    {
        for (unsigned i=0; i<ALGO_bandCnt; i++)
        {
            /* Add band to dictionary of bands but don't override if already there */
            const char* band = ALGO_bands[i];
            auto it = bandsDict.find(band);
            if(it == bandsDict.end())
                bandsDict[band] = false;
        }
    }

    /* If user specified flags, add group's Fmask to dictionary of bands */
    if(parms->flags_file)
    {
        /* Add band to dictionary of bands but don't override if already there */
        const char* band = "Fmask";
        auto it = bandsDict.find(band);
        if(it == bandsDict.end())
            bandsDict[band] = false;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LandsatHlsRaster::~LandsatHlsRaster(void)
{
    VSIUnlink(indexFile.c_str());
}

/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexFile(const OGRGeometry* geo, std::string& file, const std::vector<point_info_t>* points)
{
    static_cast<void>(geo);
    static_cast<void>(points);
    file = indexFile;
    // mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::findRasters(raster_finder_t* finder)
{
    const std::vector<OGRFeature*>* flist = finder->featuresList;
    const OGRGeometry* geo = finder->geo;
    const uint32_t start   = 0;
    const uint32_t end     = flist->size();

    try
    {
        for(uint32_t i = start; i < end; i++)
        {
            OGRFeature* feature = flist->at(i);
            OGRGeometry *rastergeo = feature->GetGeometryRef();

            if (!rastergeo->Intersects(geo)) continue;

            /* Set raster group time and group featureId */
            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->featureId = StringLib::duplicate(feature->GetFieldAsString("id"));
            rgroup->gpsTime = getGmtDate(feature, DATE_TAG, rgroup->gmtDate);

            /* Find each requested band in the index file */
            for(auto it = bandsDict.begin(); it != bandsDict.end(); it++)
            {
                const char* bandName = it->first.c_str();

                /* skip algo names (NDIS, etc) */
                if(validAlgoName(bandName))
                    continue;

                const char* fname = feature->GetFieldAsString(bandName);
                if(fname && strlen(fname) > 0)
                {
                    const std::string fileName(fname);
                    const size_t pos = strlen(URL_str);

                    raster_info_t rinfo;
                    rinfo.fileId = finder->fileDict.add(filePath + fileName.substr(pos));

                    if(strcmp(bandName, "Fmask") == 0)
                    {
                        /* Use base class generic flags tag */
                        rinfo.flagsBandNum = 1;
                        rinfo.tag = FLAGS_TAG;
                        if(parms->flags_file)
                        {
                            rgroup->infovect.push_back(rinfo);
                        }
                    }
                    else
                    {
                        rinfo.tag = bandName;
                        rgroup->infovect.push_back(rinfo);
                    }
                }
            }

            // mlog(DEBUG, "Added group: %s with %ld rasters", rgroup->featureId, rgroup->infovect.size());
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


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * validateBandNames
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::validateBandNames(void)
{
    bool valid = false;

    for (int i = 0; i < parms->bands.length(); i++)
    {
        const std::string& bandName = parms->bands[i];
        valid = validL8Band(bandName.c_str()) || validS2Band(bandName.c_str()) || validAlgoName(bandName.c_str());

        /* User specified invalid band name */
        if(!valid) mlog(ERROR, "Invalid band name: %s", bandName.c_str());
    }

    return valid;
}

/*----------------------------------------------------------------------------
 * validateBand
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::validateBand(band_type_t type, const char* bandName)
{
    const char** tags;
    int tagsCnt = 0;

    if(bandName == NULL) return false;

    if(type == SENTINEL2)
    {
        tags    = S2_bands;
        tagsCnt = S2_bandCnt;
    }
    else if(type == LANDSAT8)
    {
        tags    = L8_bands;
        tagsCnt = L8_bandCnt;
    }
    else if(type == ALGOBAND)
    {
        tags    = ALGO_bands;
        tagsCnt = ALGO_bandCnt;
    }
    else if(type == ALGONAME)
    {
        tags    = ALGO_names;
        tagsCnt = ALGO_nameCnt;
    }

    for(int i = 0; i < tagsCnt; i++)
    {
        const char* tag = tags[i];
        if( strncasecmp(bandName, tag, strlen(tag)) == 0)
            return true;
    }

    return false;
}


uint32_t LandsatHlsRaster::_getGroupSamples(sample_mode_t mode, const rasters_group_t* rgroup,
                                            List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx)
{
    uint32_t errors = SS_NO_ERRORS;

    /* Which group is it? Landsat8 or Sentinel2 */
    bool isL8 = false;
    bool isS2 = false;
    std::size_t pos;

    const std::string featureId = rgroup->featureId;
    pos = featureId.find("HLS.L30");
    if(pos != std::string::npos) isL8 = true;

    pos = featureId.find("HLS.S30");
    if(pos != std::string::npos) isS2 = true;

    if(!isL8 && !isS2)
        throw RunTimeException(DEBUG, RTE_ERROR, "Could not find valid Landsat8/Sentinel2 groupId");

    const double invalid = -999999.0;
    double green;
    double red;
    double nir08;
    double swir16;
    green = red = nir08 = swir16 = invalid;

    /* Landsat rasters use only the first inner band */
    const int INNER_BAND_INDX = 0;

    /* Samples to be returned to the user */
    std::vector<RasterSample*> sampleVect;

    /* Collect samples for all rasters */
    if(mode == SERIAL)
    {
        for(const auto& rinfo : rgroup->infovect)
        {
            const char* key = fileDictGet(rinfo.fileId);
            cacheitem_t* item;
            if(cache.find(key, &item))
            {
                RasterSample* sample = item->bandSample[INNER_BAND_INDX];

                /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                if(sample == NULL) continue;;

                sample->flags = flags;

                /* Is this band's sample to be returned to the user? */
                const char* bandName = rinfo.tag.c_str();
                auto it = bandsDict.find(bandName);
                if(it != bandsDict.end())
                {
                    const bool returnBandSample = it->second;
                    if(returnBandSample)
                    {
                        sample->bandName = bandName;
                        sampleVect.push_back(sample);
                        item->bandSample[INNER_BAND_INDX] = NULL;
                    }
                }

                /* green and red bands are the same for L8 and S2 */
                if(rinfo.tag == "B03") green = sample->value;
                if(rinfo.tag == "B04") red = sample->value;

                if(isL8)
                {
                    if(rinfo.tag == "B05") nir08 = sample->value;
                    if(rinfo.tag == "B06") swir16 = sample->value;
                }
                else /* Must be Sentinel2 */
                {
                    if(rinfo.tag == "B8A") nir08 = sample->value;
                    if(rinfo.tag == "B11") swir16 = sample->value;
                }
            }
        }
    }
    else if(mode == BATCH)
    {
        for(const auto& rinfo : rgroup->infovect)
        {
            /* This is the unique raster we are looking for, it cannot be NULL */
            unique_raster_t* ur = rinfo.uraster;
            assert(ur);

            /* Get the sample for this point from unique raster */
            for(point_sample_t& ps : ur->pointSamples)
            {
                if(ps.pointIndex == pointIndx)
                {
                    RasterSample* sample = ps.bandSample[INNER_BAND_INDX];

                    /* sample can be NULL if raster read failed, (e.g. point out of bounds) */
                    if(sample == NULL) break;

                    /* Is this band's sample to be returned to the user? */
                    const char* bandName = rinfo.tag.c_str();

                    auto it = bandsDict.find(bandName);
                    if(it != bandsDict.end())
                    {
                        const bool returnBandSample = it->second;
                        if(returnBandSample)
                        {
                            RasterSample* s;
                            if(!ps.bandSampleReturned[INNER_BAND_INDX]->exchange(true))
                            {
                                s = ps.bandSample[INNER_BAND_INDX];
                            }
                            else
                            {
                                /* Sample has already been returned, must create a copy */
                                s = new RasterSample(*ps.bandSample[INNER_BAND_INDX]);
                            }

                            /* Set band name for this sample */
                            s->bandName = bandName;

                            /* Set time for this sample */
                            s->time = rgroup->gpsTime / 1000;

                            /* Set flags for this sample, add it to the list */
                            s->flags = flags;
                            sampleVect.push_back(sample);
                            errors |= ps.ssErrors;
                        }
                    }
                    errors |= ps.ssErrors;

                    /* green and red bands are the same for L8 and S2 */
                    if(rinfo.tag == "B03") green = sample->value;
                    if(rinfo.tag == "B04") red   = sample->value;

                    if(isL8)
                    {
                        if(rinfo.tag == "B05") nir08  = sample->value;
                        if(rinfo.tag == "B06") swir16 = sample->value;
                    }
                    else /* Must be Sentinel2 */
                    {
                        if(rinfo.tag == "B8A") nir08  = sample->value;
                        if(rinfo.tag == "B11") swir16 = sample->value;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        throw RunTimeException(DEBUG, RTE_ERROR, "Invalid sample mode");
    }

    const double groupTime = rgroup->gpsTime / 1000;
    const std::string groupName = featureId + " {\"algo\": \"";

    /* Calculate algos - make sure that all the necessary bands were read */
    if(ndsi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDict.add(groupName + "NDSI\"}"));
        if((green != invalid) && (swir16 != invalid))
            sample->value = (green - swir16) / (green + swir16);
        else sample->value = invalid;

        /* Set band name for this sample */
        sample->bandName = "NDSI";
        sampleVect.push_back(sample);
    }

    if(ndvi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDict.add(groupName + "NDVI\"}"));
        if((red != invalid) && (nir08 != invalid))
            sample->value = (nir08 - red) / (nir08 + red);
        else sample->value = invalid;

        /* Set band name for this sample */
        sample->bandName = "NDVI";
        sampleVect.push_back(sample);
    }

    if(ndwi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDict.add(groupName + "NDWI\"}"));
        if((nir08 != invalid) && (swir16 != invalid))
            sample->value = (nir08 - swir16) / (nir08 + swir16);
        else sample->value = invalid;

        /* Set band name for this sample */
        sample->bandName = "NDWI";
        sampleVect.push_back(sample);
    }

    /* Add samples to the slist in the order of bands specified by the user in parms */
    for(int i = 0; i < parms->bands.length(); i++)
    {
        for(RasterSample* sample : sampleVect)
        {
            if(strcasecmp(parms->bands[i].c_str(), sample->bandName.c_str()) == 0)
            {
                slist->add(sample);
                break;
            }
        }
    }

    return errors;
}
