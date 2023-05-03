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
#include "TimeLib.h"

#include <strings.h>
#include <uuid/uuid.h>
#include <cstring>

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/* Landsat 8 */
const char* LandsatHlsRaster::L8_bands[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B09", "B10", "B11",
                                           "SAA", "SZA", "VAA", "VZA", BITMASK_FILE};
/* Sentinel 2 */
const char* LandsatHlsRaster::S2_bands[] = {"B01", "B02", "B03", "B04", "B05",
                                           "B06", "B07", "B08", "B09", "B10",
                                           "B11", "B12", "B8A", "SAA", "SZA",
                                           "VAA", "VZA", BITMASK_FILE};
/* Algorithm names (not real bands) */
const char* LandsatHlsRaster::ALGO_names[] = {"NDSI", "NDVI", "NDWI"};

/* Algorithm bands for L8 and S2 combined */
const char* LandsatHlsRaster::ALGO_bands[] = {"B03", "B04", "B05", "B06", "B8A", "B11"};


#define L8_bandCnt   (sizeof (L8_bands)   / sizeof (const char *))
#define S2_bandCnt   (sizeof (S2_bands)   / sizeof (const char *))
#define ALGO_nameCnt (sizeof (ALGO_names) / sizeof (const char *))
#define ALGO_bandCnt (sizeof (ALGO_bands) / sizeof (const char *))


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
    VctRaster(L, _parms)
{
    char uuid_str[UUID_STR_LEN];

    ndsi = ndvi = ndwi = false;

    if(_parms->catalog == NULL)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty CATALOG/geojson index file received");

    if(_parms->bands.length() == 0)
        throw RunTimeException(ERROR, RTE_ERROR, "Empty BANDS array received");

    filePath.append(_parms->asset->getPath()).append("/");
    indexFile = "/vsimem/" + std::string(getUUID(uuid_str)) + ".geojson";

    /* Create in memory index file (geojson) */
    VSILFILE* fp = VSIFileFromMemBuffer(indexFile.c_str(), (GByte*)_parms->catalog, (vsi_l_offset)strlen(_parms->catalog), FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);

    /* Create dictionary of bands and algo names to process */
    bool returnBandSample;
    for(int i = 0; i < _parms->bands.length(); i++)
    {
        const char* name = _parms->bands[i];
        if( isValidL8Band(name) || isValidS2Band(name) || isValidAlgoName(name))
        {
            if(!bandsDict.find(name, &returnBandSample))
            {
                returnBandSample = true;
                bandsDict.add(name, returnBandSample);
            }

            if(strcasecmp(_parms->bands[i], "NDSI") == 0) ndsi = true;
            if(strcasecmp(_parms->bands[i], "NDVI") == 0) ndvi = true;
            if(strcasecmp(_parms->bands[i], "NDWI") == 0) ndwi = true;
        }
    }

    /* If user specified only algortithm(s), add needed bands to dictionary of bands */
    if(ndsi || ndvi || ndwi)
    {
        for (int i=0; i<ALGO_bandCnt; i++)
        {
            const char* band = ALGO_bands[i];
            if(!bandsDict.find(band, &returnBandSample))
            {
                returnBandSample = false;
                bandsDict.add(band, returnBandSample);
            }
        }
    }

    /* If user specified flags, add group Fmask to dictionary of bands */
    if(_parms->flags_file)
    {
        const char* band = BITMASK_FILE;
        if(!bandsDict.find(band, &returnBandSample))
        {
            returnBandSample = false;
            bandsDict.add(band, returnBandSample);
        }
    }
}


/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexFile(std::string& file, double lon, double lat)
{
    // file = "/data/hls/hls_trimmed.geojson";
    file = indexFile;
    mlog(DEBUG, "Using %s", file.c_str());
}


/*----------------------------------------------------------------------------
 * getIndexBbox
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getIndexBbox(bbox_t &bbox, double lon, double lat)
{
    OGREnvelope env;
    OGRErr err = layer->GetExtent(&env);

    bbox.lon_min = env.MinX;
    bbox.lat_min = env.MinY;
    bbox.lon_max = env.MaxX;
    bbox.lat_max = env.MaxY;
}


/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool LandsatHlsRaster::findRasters(OGRPoint& p)
{
    double t1 = TimeLib::latchtime();
    try
    {
        rasterGroupList->clear();

        /* For now assume the first layer has the feature we need */
        layer->ResetReading();
        while (OGRFeature* feature = layer->GetNextFeature())
        {
            OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);

            if(!geo->Contains(&p)) continue;

            rasters_group_t rgroup;
            rgroup.id = feature->GetFieldAsString("id");
            double gps = getGmtDate(feature, "datetime", rgroup.gmtDate);
            rgroup.gpsTime = gps;

            /* Find each requested band in the index file */
            const std::string fileToken = "HLS";
            bool val;
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
                    std::string fileName(fname);
                    std::size_t pos = fileName.find(fileToken);
                    if(pos == std::string::npos)
                        throw RunTimeException(DEBUG, RTE_ERROR, "Could not find marker %s in file", fileToken.c_str());

                    fileName = filePath + fileName.substr(pos);

                    raster_info_t rinfo;
                    rinfo.fileName = fileName;
                    rinfo.tag = bandName;
                    rinfo.gpsTime = rgroup.gpsTime;
                    rinfo.gmtDate = rgroup.gmtDate;

                    rgroup.list.add(rgroup.list.length(), rinfo);
                }
                bandName = bandsDict.next(&val);
            }

            mlog(DEBUG, "Added group: %s with %ld rasters", rgroup.id.c_str(), rgroup.list.length());
            OGRFeature::DestroyFeature(feature);
            rasterGroupList->add(rasterGroupList->length(), rgroup);
        }
        mlog(DEBUG, "Found %ld raster groups for (%.2lf, %.2lf)", rasterGroupList->length(), p.getX(), p.getY());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }
    double t2 = TimeLib::latchtime();
    print2term("%-20s %.10lf\n", __FUNCTION__, t2 - t1 );

    return (rasterGroupList->length() > 0);
}


/*----------------------------------------------------------------------------
 * getGroupSamples
 *----------------------------------------------------------------------------*/
void LandsatHlsRaster::getGroupSamples (const rasters_group_t& rgroup, List<sample_t>& slist, uint32_t flags)
{
    /* Which group is it? Landsat8 or Sentinel2 */
    bool isL8 = false;
    bool isS2 = false;
    std::size_t pos;

    pos = rgroup.id.find("HLS.L30");
    if(pos != std::string::npos) isL8 = true;

    pos = rgroup.id.find("HLS.S30");
    if(pos != std::string::npos) isS2 = true;

    if(!isL8 && !isS2)
        throw RunTimeException(DEBUG, RTE_ERROR, "Could not find valid Landsat8/Sentinel2 groupId");

    double invalid = -999999.0;
    double green, red, nir08, swir16;
    green = red = nir08 = swir16 = invalid;

    /* Collect samples for all rasters */
    Ordering<raster_info_t>::Iterator raster_iter(rgroup.list);
    for(int j = 0; j < raster_iter.length; j++)
    {
        const raster_info_t& rinfo = raster_iter[j].value;
        const char* key            = rinfo.fileName.c_str();
        Raster* raster             = NULL;
        if(rasterDict.find(key, &raster))
        {
            assert(raster);
            if(raster->enabled && raster->sampled)
            {
                /* Update dictionary of used raster files */
                raster->sample.fileId = fileDictAdd(raster->fileName);
                raster->sample.flags  = flags;

                /* Is this band's sample to be returned to the user? */
                bool returnBandSample = false;
                const char* bandName  = rinfo.tag.c_str();
                if(bandsDict.find(bandName, &returnBandSample))
                {
                    if(returnBandSample)
                        slist.add(raster->sample);
                }

                /* green and red bands are the same for L8 and S2 */
                if(rinfo.tag == "B03") green = raster->sample.value;
                if(rinfo.tag == "B04") red = raster->sample.value;

                if(isL8)
                {
                    if(rinfo.tag == "B05") nir08 = raster->sample.value;
                    if(rinfo.tag == "B06") swir16 = raster->sample.value;
                }
                else /* Must be Sentinel2 */
                {
                    if(rinfo.tag == "B8A") nir08 = raster->sample.value;
                    if(rinfo.tag == "B11") swir16 = raster->sample.value;
                }
            }
        }
    }

    sample_t sample;
    std::string groupName = rgroup.id + " {\"algo\": \"";

    /* Calculate algos - make sure that all the necessary bands were read */
    if(ndsi)
    {
        bzero(&sample, sizeof(sample));
        if((green != invalid) && (swir16 != invalid))
            sample.value = (green - swir16) / (green + swir16);
        else sample.value = invalid;

        sample.fileId = fileDictAdd(groupName + "NDSI\"}");
        slist.add(sample);
    }

    if(ndvi)
    {
        bzero(&sample, sizeof(sample));
        if((red != invalid) && (nir08 != invalid))
            sample.value = (nir08 - red) / (nir08 + red);
        else sample.value = invalid;

        sample.fileId = fileDictAdd(groupName + "NDVI\"}");
        slist.add(sample);
    }

    if(ndwi)
    {
        bzero(&sample, sizeof(sample));
        if((nir08 != invalid) && (swir16 != invalid))
            sample.value = (nir08 - swir16) / (nir08 + swir16);
        else sample.value = invalid;

        sample.fileId = fileDictAdd(groupName + "NDWI\"}");
        slist.add(sample);
    }
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