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
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

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
 * PUBLIC METHODS
 ******************************************************************************/

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LandsatHlsRaster::LandsatHlsRaster(lua_State *L, GeoParms* _parms):
 GeoIndexedRaster(L, _parms),
 filePath(_parms->asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    ndsi = ndvi = ndwi = false;

    if(_parms->catalog == NULL)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty CATALOG/geojson index file received");

    if(_parms->bands->length == 0)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty BANDS array received");

    /* Create in memory index file (geojson) */
    GByte* bytes = const_cast<GByte*>(reinterpret_cast<const GByte*>(_parms->catalog));
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(), bytes, (vsi_l_offset)strlen(_parms->catalog), FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);

    /* Create dictionary of bands and algo names to process */
    bool returnBandSample;
    for(int i = 0; i < _parms->bands->length; i++)
    {
        const char* name = (*_parms->bands)[i].c_str();
        if( isValidL8Band(name) || isValidS2Band(name) || isValidAlgoName(name))
        {
            if(!bandsDict.find(name, &returnBandSample))
            {
                returnBandSample = true;
                bandsDict.add(name, returnBandSample);
            }

            if(strcasecmp((*_parms->bands)[i].c_str(), "NDSI") == 0) ndsi = true;
            if(strcasecmp((*_parms->bands)[i].c_str(), "NDVI") == 0) ndvi = true;
            if(strcasecmp((*_parms->bands)[i].c_str(), "NDWI") == 0) ndwi = true;
        }
    }

    /* If user specified only algortithm(s), add needed bands to dictionary of bands */
    if(ndsi || ndvi || ndwi)
    {
        for (unsigned i=0; i<ALGO_bandCnt; i++)
        {
            const char* band = ALGO_bands[i];
            if(!bandsDict.find(band, &returnBandSample))
            {
                returnBandSample = false;
                bandsDict.add(band, returnBandSample);
            }
        }
    }

    /* If user specified flags, add group's Fmask to dictionary of bands */
    if(_parms->flags_file)
    {
        const char* band = "Fmask";
        if(!bandsDict.find(band, &returnBandSample))
        {
            returnBandSample = false;
            bandsDict.add(band, returnBandSample);
        }
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
void LandsatHlsRaster::getIndexFile(const OGRGeometry* geo, std::string& file)
{
    static_cast<void>(geo);
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::findRasters(finder_t* finder)
{
    const OGRGeometry* geo    = finder->geo;
    const uint32_t start_indx = finder->range.start_indx;
    const uint32_t end_indx   = finder->range.end_indx;

    try
    {
        for(uint32_t i = start_indx; i < end_indx; i++)
        {
            OGRFeature* feature = featuresList[i];
            OGRGeometry *rastergeo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if (!rastergeo->Intersects(geo)) continue;

            /* Set raster group time and group id */
            rasters_group_t* rgroup = new rasters_group_t;
            rgroup->infovect.reserve(MAX_LANDSAT_RASTER_GROUP_SIZE);
            rgroup->id = feature->GetFieldAsString("id");
            rgroup->gpsTime = getGmtDate(feature, DATE_TAG, rgroup->gmtDate);

            /* Find each requested band in the index file */
            bool val;

            bandsDictMutex.lock();
            const char* bandName = bandsDict.first(&val);
            while(bandName != NULL)
            {
                /* skip algo names (NDIS, etc) */
                if(isValidAlgoName(bandName))
                {
                    bandName = bandsDict.next(&val);
                    continue;
                }

                const char* fname = feature->GetFieldAsString(bandName);
                if(fname && strlen(fname) > 0)
                {
                    const std::string fileName(fname);
                    const size_t pos = strlen(URL_str);

                    raster_info_t rinfo;
                    rinfo.dataIsElevation = false; /* All bands are not elevation */
                    rinfo.fileName = filePath + fileName.substr(pos);

                    if(strcmp(bandName, "Fmask") == 0)
                    {
                        /* Use base class generic flags tag */
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
                bandName = bandsDict.next(&val);
            }
            bandsDictMutex.unlock();

            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup->id.c_str(), rgroup->infovect.size());
            finder->rasterGroups.push_back(rgroup);
        }
        mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/*----------------------------------------------------------------------------
 * getGroupSamples
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getGroupSamples (const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags)
{
    /* Which group is it? Landsat8 or Sentinel2 */
    bool isL8 = false;
    bool isS2 = false;
    std::size_t pos;

    pos = rgroup->id.find("HLS.L30");
    if(pos != std::string::npos) isL8 = true;

    pos = rgroup->id.find("HLS.S30");
    if(pos != std::string::npos) isS2 = true;

    if(!isL8 && !isS2)
        throw RunTimeException(DEBUG, RTE_ERROR, "Could not find valid Landsat8/Sentinel2 groupId");

    const double invalid = -999999.0;
    double green;
    double red;
    double nir08;
    double swir16;
    green = red = nir08 = swir16 = invalid;

    /* Collect samples for all rasters */
    for(const auto& rinfo: rgroup->infovect)
    {
        const char* key = rinfo.fileName.c_str();
        cacheitem_t* item;
        if(cache.find(key, &item))
        {
            RasterSample* sample = item->sample;
            if(sample)
            {
                sample->flags  = flags;

                /* Is this band's sample to be returned to the user? */
                bool returnBandSample = false;
                const char* bandName  = rinfo.tag.c_str();
                if(bandsDict.find(bandName, &returnBandSample))
                {
                    if(returnBandSample)
                    {
                        slist.add(sample);
                        item->sample = NULL;
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

    const double groupTime = rgroup->gpsTime / 1000;
    const std::string groupName = rgroup->id + " {\"algo\": \"";

    /* Calculate algos - make sure that all the necessary bands were read */
    if(ndsi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDictAdd(groupName + "NDSI\"}"));
        if((green != invalid) && (swir16 != invalid))
            sample->value = (green - swir16) / (green + swir16);
        else sample->value = invalid;
        slist.add(sample);
    }

    if(ndvi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDictAdd(groupName + "NDVI\"}"));
        if((red != invalid) && (nir08 != invalid))
            sample->value = (nir08 - red) / (nir08 + red);
        else sample->value = invalid;
        slist.add(sample);
    }

    if(ndwi)
    {
        RasterSample* sample = new RasterSample(groupTime, fileDictAdd(groupName + "NDWI\"}"));
        if((nir08 != invalid) && (swir16 != invalid))
            sample->value = (nir08 - swir16) / (nir08 + swir16);
        else sample->value = invalid;
        slist.add(sample);
    }
}



/*----------------------------------------------------------------------------
 * getMaxBatchThreads
 *----------------------------------------------------------------------------*/
uint32_t LandsatHlsRaster::getMaxBatchThreads(void)
{
    /*
     * The average number of strips for a point is ~10
     * Limit the number of batch threads to 8.
     */
    return 8;
}



/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

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