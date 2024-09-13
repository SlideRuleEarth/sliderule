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

        /* Create  of points to sample */
        List<RasterObject::point_info_t*> points2sample;
        for(long i = 0; i < pointsCnt; i++)
        {
            RasterObject::point_info_t* point = new RasterObject::point_info_t();
            point->point.x = lon;
            point->point.y = lat;
            point->point.z = 0.0;
            point->gps = 0.0;

            points2sample.add(point);

            lon += lonIncr;
            lat += latIncr;
        }

        print2term("Points to sample: %ld\n", pointsCnt);
        print2term("Starting at (%.4lf, %.4lf), incrementing by (%+.4lf, %+.4lf)\n", lon, lat, lonIncr, latIncr);

        /* Get samples using serial method */
        print2term("Getting samples using serial method\n");
        List<RasterObject::sample_list_t*> serial_sllist;
        const double serialStartTime = TimeLib::latchtime();
        for(int i = 0; i < points2sample.length(); i++)
        {
            RasterObject::sample_list_t* slist = new RasterObject::sample_list_t();
            lua_obj->raster->getSamples(points2sample[i]->point, 0, *slist, NULL);

            /* Add to list */
            serial_sllist.add(slist);
        }
        const double serialStopTime = TimeLib::latchtime();

        /* Get samples using batch method */
        print2term("Getting samples using batch method\n");
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
        uint32_t validStreamSamples = 0;
        uint32_t validBatchSamples = 0;
        uint32_t nanStreamSamples = 0;
        uint32_t nanBatchSamples = 0;

        print2term("Comparing samples\n");
        if(serial_sllist.length() != batch_sllist.length())
        {
            print2term("Number of samples differ, serial: %d, batch: %d\n", serial_sllist.length(), batch_sllist.length());
            errors++;
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

                const char* serialName = lua_obj->raster->fileDictGetFile(serial->fileId);
                const char* batchName  = lua_obj->raster->fileDictGetFile(batch->fileId);

                if (strcmp(serialName, batchName) != 0)
                {
                    print2term("Files differ:\n");
                    print2term("Serial: %s\n", serialName);
                    print2term("Batch:  %s\n", batchName);
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

                if(std::isnan(serial->value)) nanStreamSamples++;
                else validStreamSamples++;

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

        print2term("Stream samples, valid: %u, nan: %u\n", validStreamSamples, nanStreamSamples);
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

