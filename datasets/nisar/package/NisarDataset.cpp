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

/* Valid Bands for BlueTopo Bathy Raster */
const char* NisarDataset::validBands[] = {"azimuthOffset", "rangeOffset"};  // For now we only support these
#define validBandsCnt (sizeof (validBands) / sizeof (const char *))

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
NisarDataset::NisarDataset(lua_State* L, RequestFields* rqst_parms, const char* key):
 GeoIndexedRaster(L, rqst_parms, key),
 filePath(parms->asset.asset->getPath()),
 indexFile("/vsimem/" + GdalRaster::getUUID() + ".geojson")
{
    if(!validateBandNames())
    {
        throw RunTimeException(DEBUG, RTE_FAILURE, "Invalid band name specified");
    }

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
    static_cast<void>(finder);

    const std::string h5file = "/vsis3/sds-n-cumulus-prod-nisar-sample-data/GOFF/NISAR_L2_PR_GOFF_001_030_A_019_002_2000_SH_20081012T060911_20081012T060925_20081127T061000_20081127T061014_D00404_N_F_J_001/NISAR_L2_PR_GOFF_001_030_A_019_002_2000_SH_20081012T060911_20081012T060925_20081127T061000_20081127T061014_D00404_N_F_J_001.h5";
    // const std::string h5file = "/data/NISAR/NISAR_L2_PR_GOFF_001_030_A_019_002_2000_SH_20081012T060911_20081012T060925_20081127T061000_20081127T061014_D00404_N_F_J_001.h5"
    // const std::string ds1 = "HDF5:\"" + h5file + "\"" + "://science/LSAR/GCOV/metadata/radarGrid/alongTrackUnitVectorX";
    // const std::string ds2 = "HDF5:\"" + h5file + "\"" + "://science/LSAR/GCOV/metadata/radarGrid/alongTrackUnitVectorY";

    const std::string ds1 = R"(HDF5:"/vsis3/sds-n-cumulus-prod-nisar-sample-data/GOFF/NISAR_L2_PR_GOFF_001_030_A_019_002_2000_SH_20081012T060911_20081012T060925_20081127T061000_20081127T061014_D00404_N_F_J_001/NISAR_L2_PR_GOFF_001_030_A_019_002_2000_SH_20081012T060911_20081012T060925_20081127T061000_20081127T061014_D00404_N_F_J_001.h5"://science/LSAR/GOFF/grids/frequencyA/pixelOffsets/HH/layer1/alongTrackOffset)";
    const std::vector<std::string> datasets = {ds1};

    try
    {
        for(uint32_t i = 0; i < datasets.size(); i++)
        {
            rasters_group_t* rgroup = new rasters_group_t;
            // rgroup->gpsTime = getGmtDate(feature, "Delivered_Date", rgroup->gmtDate) / 1000.0;

            const std::string& fullPath = datasets[i];

            raster_info_t rinfo;
            rinfo.elevationBandNum = 0;
            rinfo.tag = VALUE_TAG;
            rinfo.fileId = finder->fileDict.add(fullPath);
            rgroup->infovect.push_back(rinfo);
            rgroup->infovect.shrink_to_fit();

            mlog(DEBUG, "Added group with %ld rasters", rgroup->infovect.size());
            for(unsigned j = 0; j < rgroup->infovect.size(); j++)
                mlog(DEBUG, "  %s", finder->fileDict.get(rgroup->infovect[j].fileId));

            // Add the group
            finder->rasterGroups.push_back(rgroup);
        }
        finder->rasterGroups.shrink_to_fit();

        mlog(DEBUG, "Found %ld raster groups", finder->rasterGroups.size());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting time from raster feature file: %s", e.what());
    }

    return (!finder->rasterGroups.empty());
}


/*----------------------------------------------------------------------------
 * getGmtDate
 *----------------------------------------------------------------------------*/
double NisarDataset::getGmtDate(const OGRFeature* feature, const char* field, TimeLib::gmt_time_t& gmtDate)
{
    const int i = feature->GetFieldIndex(field);
    if(i == -1)
    {
        mlog(ERROR, "Time field: %s not found, unable to get GMT date", field);
        return 0;
    }

    double gpstime = 0;
    double seconds;
    int year;
    int month;
    int day;
    int hour;
    int minute;

    /*
    * The raster's 'Delivered_Date' in the GPKG index file is not in ISO8601 format.
    * Instead, it uses the format "YYYY-MM-DD HH:MM:SS".
    */
    if(const char* dateStr = feature->GetFieldAsString(i))
    {
        if(sscanf(dateStr, "%04d-%02d-%02d %02d:%02d:%lf",
            &year, &month, &day, &hour, &minute, &seconds) == 6)
        {
            gmtDate.year = year;
            gmtDate.doy = TimeLib::dayofyear(year, month, day);
            gmtDate.hour = hour;
            gmtDate.minute = minute;
            gmtDate.second = seconds;
            gmtDate.millisecond = 0;
            gpstime = TimeLib::gmt2gpstime(gmtDate);
            // mlog(DEBUG, "%04d:%02d:%02d:%02d:%02d:%02d", year, month, day, hour, minute, (int)seconds);
        }
        else mlog(DEBUG, "Unable to parse date string [%s]", dateStr);
    }
    else mlog(DEBUG, "Date field is invalid");

    return gpstime;
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * validateBandNames
 *----------------------------------------------------------------------------*/
bool NisarDataset::validateBandNames(void)
{
    if(parms->bands.length() == 0)
    {
        mlog(ERROR, "No bands specified");
        return false;
    }

    for (int i = 0; i < parms->bands.length(); i++)
    {
        bool valid = false;
        for(size_t j = 0; j < validBandsCnt; j++)
        {
            /* Raster band names are case insensitive */
            if(strcmp(parms->bands[i].c_str(), validBands[j]) == 0)
            {
                valid = true;
                break;
            }
        }

        if(!valid)
        {
            mlog(ERROR, "Invalid band name: %s", parms->bands[i].c_str());
            return false;
        }
    }

    return true;
}