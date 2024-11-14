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

#include "core.h"
#include "GdalRaster.h"
#include "UT_RasterSample.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_RasterSample::OBJECT_TYPE = "UT_RasterSample";

const char* UT_RasterSample::LUA_META_NAME = "UT_RasterSample";
const struct luaL_Reg UT_RasterSample::LUA_META_TABLE[] = {
    {"test",         luaSampleTest},
    {NULL,           NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :test(<raster>)
 *----------------------------------------------------------------------------*/
int UT_RasterSample::luaCreate (lua_State* L)
{
    RasterObject* _raster = NULL;
    try
    {
        /* Get Parameters */
        _raster = dynamic_cast<RasterObject*>(getLuaObject(L, 1, RasterObject::OBJECT_TYPE));

        return createLuaObject(L, new UT_RasterSample(L, _raster));
    }
    catch(const RunTimeException& e)
    {
        if(_raster) _raster->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_RasterSample::UT_RasterSample (lua_State* L, RasterObject* _raster):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    raster(_raster)
{
    assert(raster);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_RasterSample::~UT_RasterSample(void)
{
    raster->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * ReadPointsFile
 *----------------------------------------------------------------------------*/
bool UT_RasterSample::ReadPointsFile(std::vector<RasterObject::point_info_t>& points, const char* filePath)
{
    FILE* file = fopen(filePath, "r");
    if (!file) {
        print2term("Error: Unable to open file: %s\n", filePath);
        return false;
    }

    double lon, lat;

    while (fscanf(file, "%lf %lf", &lon, &lat) == 2)
    {
        RasterObject::point_info_t pointInfo;
        pointInfo.point.x = lon;
        pointInfo.point.y = lat;
        pointInfo.point.z = 0.0;
        pointInfo.gps = 0.0;

        points.push_back(pointInfo);
    }

    fclose(file);
    return true;
}

/*----------------------------------------------------------------------------
 * TestFileDictionary
 *----------------------------------------------------------------------------*/
bool UT_RasterSample::TestFileDictionary(RasterObject* raster)
{
    /* Make a copy so we don't change the original dictionary */
    RasterFileDictionary dict = raster->fileDictCopy();

    /* Clear the dictionary but keep keySpace */
    dict.clear();

    const char* raster1 = "RasterOneNotSample";
    const char* raster2 = "RasterTwoSample";
    const uint64_t fileIdRaster1 = dict.add(raster1);
    const uint64_t fileIdRaster2 = dict.add(raster2, true);

    const std::set<uint64_t>& sampleIds = dict.getSampleIds();
    uint32_t cnt = 0;
    const char* raserName = NULL;
    uint64_t fileId;
    for (std::set<uint64_t>::const_iterator it = sampleIds.begin(); it != sampleIds.end(); it++)
    {
        fileId = *it;
        raserName = dict.get(fileId);
        cnt++;
    }

    if(cnt != 1)
    {
        mlog(CRITICAL, "Expected 1 sample but got %d", cnt);
        return false;
    }

    if(strcmp(raserName, raster2) != 0)
    {
        mlog(CRITICAL, "Expected %s but got %s", raster2, raserName);
        return false;
    }

    const uint64_t keySpace1 = fileIdRaster1 >> 32;
    const uint64_t keySpace2 = fileIdRaster2 >> 32;

    if(keySpace1 != keySpace2)
    {
        mlog(CRITICAL, "Expected key space %lu but got %lu", keySpace1, keySpace2);
        return false;
    }

    dict.setSample(fileIdRaster1);  // Set raster1 as sample
    cnt = 0;
    for (std::set<uint64_t>::const_iterator it = sampleIds.begin(); it != sampleIds.end(); it++)
    {
        cnt++;
    }

    if(cnt != 2)
    {
        mlog(CRITICAL, "Expected 2 sample but got %d", cnt);
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * luaSampleTest
 *----------------------------------------------------------------------------*/
int UT_RasterSample::luaSampleTest(lua_State* L)
{
    bool status = false;
    uint32_t errors = 0;

    UT_RasterSample* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<UT_RasterSample*>(getLuaSelf(L, 1));

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Increments */
        const double lonIncr = getLuaFloat(L, 4);
        const double latIncr = getLuaFloat(L, 5);

        /* Get Count */
        const long pointsCnt = getLuaInteger(L, 6);

        /* Get Points File if provided */
        const char* pointsFile = getLuaString(L, 7, true, NULL);

        /* Test fileDictionary */
        if(!lua_obj->TestFileDictionary(lua_obj->raster))
        {
            return returnLuaStatus(L, false);
        }

        std::vector<RasterObject::point_info_t> points2sample;

        if(pointsFile)
        {
            std::vector<RasterObject::point_info_t> pointsInFile;
            print2term("Using points file: %s\n", pointsFile);
            if(!lua_obj->ReadPointsFile(pointsInFile, pointsFile))
            {
                return returnLuaStatus(L, false);
            }

            /* Calculate step to sample pointsCnt so they are not consecutive */
            uint32_t step = pointsInFile.size() / pointsCnt;
            if(step < 1) step = 1;
            print2term("Using step of %u\n", step);

            /* Create points to sample */
            for(uint32_t i = 0; i < pointsInFile.size(); i += step)
                points2sample.push_back(pointsInFile[i]);

            /* Trim points if needed */
            points2sample.resize(pointsCnt);
        }
        else
        {
            /* Create points to sample */
            for(long i = 0; i < pointsCnt; i++)
            {
                RasterObject::point_info_t point;
                point.point.x = lon;
                point.point.y = lat;
                point.point.z = 0.0;
                point.gps = 0.0;

                points2sample.push_back(point);

                lon += lonIncr;
                lat += latIncr;
            }
        }

        print2term("Points to sample: %zu\n", points2sample.size());
        print2term("Starting at (%.4lf, %.4lf), incrementing by (%+.4lf, %+.4lf)\n", points2sample[0].point.x, points2sample[0].point.y, lonIncr, latIncr);
        print2term("Last point: (%.4lf, %.4lf)\n", points2sample[points2sample.size() - 1].point.x, points2sample[points2sample.size() - 1].point.y);

        /* Get samples using serial method */
        print2term("Getting samples for %zu points using serial method\n", points2sample.size());
        List<RasterObject::sample_list_t*> serial_sllist;
        const double serialStartTime = TimeLib::latchtime();
        for(uint32_t i = 0; i < points2sample.size(); i++)
        {
            RasterObject::sample_list_t* slist = new RasterObject::sample_list_t();
            lua_obj->raster->getSamples(points2sample[i].point, 0, *slist, NULL);

            /* Add to list */
            serial_sllist.add(slist);
        }
        const double serialStopTime = TimeLib::latchtime();

        /* Store fileDictionary from serial sampling, batch sampling will overwrite it */
        RasterFileDictionary serialDict = lua_obj->raster->fileDictCopy();

        /* Get samples using batch method */
        print2term("Getting samples for %zu points using batch method\n", points2sample.size());
        List<RasterObject::sample_list_t*> batch_sllist;
        const double batchStartTime = TimeLib::latchtime();
        lua_obj->raster->getSamples(points2sample, batch_sllist, NULL);
        const double batchStopTime = TimeLib::latchtime();

        /* Print times */
        const double serialTime = serialStopTime - serialStartTime;
        const double batchTime = batchStopTime - batchStartTime;
        print2term("Serial time: %lf\n", serialTime);
        print2term("Batch  time: %lf\n", batchTime);

        /* Compare times */
        if (batchTime > 0)
        {
            const double performanceDifference = ((serialTime - batchTime) / batchTime) * 100.0;
            print2term("Performance difference: %.2lf%%\n", performanceDifference);
        }

        /* Compare results */
        uint32_t allSerialSamples = 0;
        uint32_t allBatchSamples = 0;
        uint32_t validSerialSamples = 0;
        uint32_t validBatchSamples = 0;
        uint32_t nanSerialSamples = 0;
        uint32_t nanBatchSamples = 0;

        /* Count all samples in each list */
        for(int i = 0; i < serial_sllist.length(); i++)
        {
            RasterObject::sample_list_t* slist = serial_sllist[i];
            for(int j = 0; j < slist->length(); j++)
                allSerialSamples++;
        }
        for(int i = 0; i < batch_sllist.length(); i++)
        {
            RasterObject::sample_list_t* slist = batch_sllist[i];
            for(int j = 0; j < slist->length(); j++)
                    allBatchSamples++;
        }

        print2term("Comparing lists\n");
        if(serial_sllist.length() != batch_sllist.length())
        {
            print2term("Number of samples differ, serial: %d, batch: %d\n", serial_sllist.length(), batch_sllist.length());
            return returnLuaStatus(L, false);
        }

        print2term("Comparing %u samples\n", allSerialSamples);
        if(allSerialSamples != allBatchSamples)
        {
            print2term("Number of samples differ, serial: %d, batch: %d\n", allSerialSamples, allBatchSamples);
            return returnLuaStatus(L, false);
        }

        for(int i = 0; i < serial_sllist.length(); i++)
        {
            RasterObject::sample_list_t* serial_slist = serial_sllist[i];
            RasterObject::sample_list_t* batch_slist = batch_sllist[i];

            if(serial_slist->length() != batch_slist->length())
            {
                print2term("Number of samples differ, serial: %d, batch: %d\n", serial_slist->length(), batch_slist->length());
                errors++;
            }

            for(int j = 0; j < serial_slist->length(); j++)
            {
                RasterSample* serial = (*serial_slist)[j];
                RasterSample* batch  = (*batch_slist)[j];

                const char* serialName = serialDict.get(serial->fileId);
                const char* batchName  = lua_obj->raster->fileDictGet(batch->fileId);

                if (strcmp(serialName, batchName) != 0)
                {
                    print2term("Files differ:\n");
                    print2term("Serial: %s\n", serialName);
                    print2term("Batch:  %s\n", batchName);
                    errors++;
                }

                const std::string& serialBand = serial->bandName;
                const std::string& batchBand  = batch->bandName;

                if (serialBand != batchBand)
                {
                    print2term("Bands differ:\n");
                    print2term("Serial: %s\n", serialBand.c_str());
                    print2term("Batch:  %s\n", batchBand.c_str());
                    errors++;
                }

                /* Compare as whole seconds */
                if(static_cast<int64_t>(serial->time) != static_cast<int64_t>(batch->time))
                {
                    print2term("Time differ: %lf != %lf\n", serial->time, batch->time);
                    errors++;
                }

                /* Nodata sample values are returned as NaN, cannot compare two NaNs */
                const bool dontCompare = std::isnan(serial->value) && std::isnan(batch->value);
                if(!dontCompare && serial->value != batch->value)
                {
                    print2term("Value differ: %lf != %lf\n", serial->value, batch->value);
                    errors++;
                }

                if(std::isnan(serial->value)) nanSerialSamples++;
                else validSerialSamples++;

                if(std::isnan(batch->value)) nanBatchSamples++;
                else validBatchSamples++;

                if(serial->stats.mean != batch->stats.mean)
                {
                    print2term("Mean differ: %lf != %lf\n", serial->stats.mean, batch->stats.mean);
                    errors++;
                }

                if(serial->stats.stdev != batch->stats.stdev)
                {
                    print2term("Stdev differ: %lf != %lf\n", serial->stats.stdev, batch->stats.stdev);
                    errors++;
                }

                if(serial->stats.mad != batch->stats.mad)
                {
                    print2term("Mad differ: %lf != %lf\n", serial->stats.mad, batch->stats.mad);
                    errors++;
                }
            }
        }

        print2term("Serial samples, valid: %u, nan: %u\n", validSerialSamples, nanSerialSamples);
        print2term("Batch  samples, valid: %u, nan: %u\n", validBatchSamples, nanBatchSamples);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        errors++;
    }

    if(errors == 0) status = true;
    else status = false;

    /* Return Status */
    return returnLuaStatus(L, status);
}

