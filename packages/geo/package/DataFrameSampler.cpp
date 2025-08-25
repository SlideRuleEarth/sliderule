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

#include "OsApi.h"
#include "TimeLib.h"
#include "RasterObject.h"
#include "DataFrameSampler.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* DataFrameSampler::OBJECT_TYPE   = "DataFrameSampler";
const char* DataFrameSampler::LUA_META_NAME = "DataFrameSampler";
const struct luaL_Reg DataFrameSampler::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - framesampler(parms)
 *----------------------------------------------------------------------------*/
int DataFrameSampler::luaCreate(lua_State* L)
{
    RequestFields* _parms = NULL;
    try
    {
        _parms  = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        return createLuaObject(L, new DataFrameSampler(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        if(_parms) _parms->releaseLuaObject();
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
DataFrameSampler::DataFrameSampler(lua_State* L, RequestFields* _parms):
    FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
    const char* key = parms->samplers.fields.first(NULL);
    while(key != NULL)
    {
        RasterObject* robj = RasterObject::cppCreate(parms, key);
        if(robj)
        {
            sampler_info_t* sampler = new sampler_info_t(key, robj, this, parms->samplers[key]);
            samplers.push_back(sampler);
            referenceLuaObject(robj);
        }
        else
        {
            mlog(CRITICAL, "Failed to create raster <%s>", key);
        }
        key = parms->samplers.fields.next(NULL);
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
DataFrameSampler::~DataFrameSampler(void)
{
    for(sampler_info_t* sampler: samplers)
    {
        sampler->robj->stopSampling();
        delete sampler;
    }
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool DataFrameSampler::run (GeoDataFrame* dataframe)
{
    // latch start time for later runtime calculation
    const double start = TimeLib::latchtime();

    const std::string& frame_crs = dataframe->getCRS();
    if(frame_crs.empty())
    {
        mlog(CRITICAL, "DataFrameSampler: incoming dataframe missing CRS");
        return false;
    }

    // populate points vector
    if(!populatePoints(dataframe))
    {
        mlog(CRITICAL, "Failed to populate points for sampling");
        return false;
    }

    // get samples for all user RasterObjects
    for(sampler_info_t* sampler: samplers)
    {
        // Cast away const on RasterObject::parms so we can set source_crs.
        const GeoFields* geoparms = sampler->robj->getGeoParms();
        GeoFields* mutable_parms = const_cast<GeoFields*>(geoparms);

        // Storing frame_crs in GeoFields is the simplest, lowest-impact way to propagate frame_crs.
        mutable_parms->source_crs = frame_crs;
        mlog(DEBUG, "DataFrameSampler: source CRS = %s", frame_crs.c_str());

        // sample the rasters
        sampler->robj->getSamples(sampler->obj->points, sampler->samples);

        // put samples into dataframe columns
        if(sampler->geoparms.force_single_sample)
        {
            populateColumns(dataframe, sampler);
        }
        else
        {
            populateMultiColumns(dataframe, sampler);
        }

        // release since not needed anymore
        sampler->samples.clear();
    }

    // update runtime and return success
    updateRunTime(TimeLib::latchtime() - start);
    return true;
}

/*----------------------------------------------------------------------------
 * populatePoints
 *----------------------------------------------------------------------------*/
bool DataFrameSampler::populatePoints (GeoDataFrame* dataframe)
{
    // get columns
    const FieldColumn<time8_t>*  t_column = dataframe->getTimeColumn();
    const FieldColumn<double>*  x_column = dataframe->getXColumn();
    const FieldColumn<double>*  y_column = dataframe->getYColumn();
    const FieldColumn<float>*   z_column = dataframe->getZColumn();

    // chech columns
    if(!x_column || !y_column)
    {
        mlog(CRITICAL, "Missing x and/or y columns (%d,%d)", x_column == NULL, y_column == NULL);
        return false;
    }

    // initialize list of points
    for(long i = 0; i < dataframe->length(); i++)
    {
        points.emplace_back(point_info_t({{0.0, 0.0, 0.0}, 0}));
    }

    // populate x and y
    for(long i = 0; i < dataframe->length(); i++)
    {
        points[i].point3d.x = (*x_column)[i];
        points[i].point3d.y = (*y_column)[i];
    }

    // populate z (optionally)
    if(z_column)
    {
        for(long i = 0; i < dataframe->length(); i++)
        {
            points[i].point3d.z = static_cast<double>((*z_column)[i]);
        }
    }

    // populate time (optionally)
    if(t_column)
    {
        for(long i = 0; i < dataframe->length(); i++)
        {
            points[i].gps = TimeLib::sysex2gpstime((*t_column)[i]);
        }
    }

    // success
    return true;
}

/*----------------------------------------------------------------------------
 * populateMultiColumns
 *----------------------------------------------------------------------------*/
bool DataFrameSampler::populateMultiColumns (GeoDataFrame* dataframe, sampler_info_t* sampler)
{
    // create standard columns
    FieldColumn<FieldList<double>>* value_column = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
    FieldColumn<FieldList<time8_t>>* time_column = new FieldColumn<FieldList<time8_t>>(Field::NESTED_LIST);
    FieldColumn<FieldList<uint64_t>>* fileid_column = new FieldColumn<FieldList<uint64_t>>(Field::NESTED_LIST);

    // create flag column
    FieldColumn<FieldList<uint32_t>>* flags_column = NULL;
    if(sampler->robj->hasFlags()) flags_column = new FieldColumn<FieldList<uint32_t>>(Field::NESTED_LIST);

    // create band column
    FieldColumn<FieldList<string>>* band_column = NULL;
    if(sampler->robj->hasBands()) band_column = new FieldColumn<FieldList<string>>(Field::NESTED_LIST);

    // create zonal stat columns
    FieldColumn<FieldList<uint32_t>>* count_column = NULL;
    FieldColumn<FieldList<double>>* min_column = NULL;
    FieldColumn<FieldList<double>>* max_column = NULL;
    FieldColumn<FieldList<double>>* mean_column = NULL;
    FieldColumn<FieldList<double>>* median_column = NULL;
    FieldColumn<FieldList<double>>* stdev_column = NULL;
    FieldColumn<FieldList<double>>* mad_column = NULL;
    if(sampler->robj->hasZonalStats())
    {
        count_column    = new FieldColumn<FieldList<uint32_t>>(Field::NESTED_LIST);
        min_column      = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        max_column      = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        mean_column     = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        median_column   = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        stdev_column    = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        mad_column      = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
    }

    // create slope derivative columns
    FieldColumn<FieldList<uint32_t>>* scount_column = NULL;
    FieldColumn<FieldList<double>>* slope_column = NULL;
    FieldColumn<FieldList<double>>* aspect_column = NULL;
    if(sampler->robj->hasSpatialDerivs())
    {
        scount_column   = new FieldColumn<FieldList<uint32_t>>(Field::NESTED_LIST);
        slope_column    = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
        aspect_column   = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
    }

    // iterate over each sample in a vector of lists of samples
    for(int i = 0; i < sampler->samples.length(); i++)
    {
        sample_list_t* slist = sampler->samples[i];

        // populate core sample fields
        FieldList<double> value_list;
        FieldList<time8_t> time_list;
        FieldList<uint64_t> fileid_list;
        FieldList<uint32_t> flags_list;
        FieldList<string> band_list;
        for(int j = 0; j < slist->length(); j++)
        {
            const RasterSample* sample = slist->get(j);
            value_list.append(sample->value);
            time_list.append(TimeLib::gps2systimeex(sample->time));
            fileid_list.append(sample->fileId);
            if(flags_column) flags_list.append(sample->flags);
            if(band_column) band_list.append(sample->bandName);
        }
        value_column->append(value_list);
        time_column->append(time_list);
        fileid_column->append(fileid_list);

        // populate zonal stats fields
        if(sampler->robj->hasZonalStats())
        {
            FieldList<uint32_t> count_list;
            FieldList<double> min_list;
            FieldList<double> max_list;
            FieldList<double> mean_list;
            FieldList<double> median_list;
            FieldList<double> stdev_list;
            FieldList<double> mad_list;
            for(int j = 0; j < slist->length(); j++)
            {
                const RasterSample* sample = slist->get(j);
                count_list.append(sample->stats.count);
                min_list.append(sample->stats.min);
                max_list.append(sample->stats.max);
                mean_list.append(sample->stats.mean);
                median_list.append(sample->stats.median);
                stdev_list.append(sample->stats.stdev);
                mad_list.append(sample->stats.mad);
            }
            assert(count_column);   count_column->append(count_list);
            assert(min_column);     min_column->append(min_list);
            assert(max_column);     max_column->append(max_list);
            assert(mean_column);    mean_column->append(mean_list);
            assert(median_column);  median_column->append(median_list);
            assert(stdev_column);   stdev_column->append(stdev_list);
            assert(mad_column);     mad_column->append(mad_list);
        }

        // populate slope derivative fields
        if(sampler->robj->hasSpatialDerivs())
        {
            FieldList<uint32_t> scount_list;
            FieldList<double> slope_list;
            FieldList<double> aspect_list;
            for(int j = 0; j < slist->length(); j++)
            {
                const RasterSample* sample = slist->get(j);
                scount_list.append(sample->derivs.count);
                slope_list.append(sample->derivs.slopeDeg);
                aspect_list.append(sample->derivs.aspectDeg);
            }
            assert(scount_column);  scount_column->append(scount_list);
            assert(slope_column);   slope_column->append(slope_list);
            assert(aspect_column);  aspect_column->append(aspect_list);
        }
    }

    // add new columns to dataframe
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column);
    dataframe->addExistingColumn(FString("%s.time_ns",  sampler->rkey).c_str(), time_column);
    dataframe->addExistingColumn(FString("%s.fileid",   sampler->rkey).c_str(), fileid_column);
    if(band_column)     dataframe->addExistingColumn(FString("%s.band",         sampler->rkey).c_str(), band_column);
    if(flags_column)    dataframe->addExistingColumn(FString("%s.flags",        sampler->rkey).c_str(), flags_column);
    if(count_column)    dataframe->addExistingColumn(FString("%s.stats.count",  sampler->rkey).c_str(), count_column);
    if(min_column)      dataframe->addExistingColumn(FString("%s.stats.min",    sampler->rkey).c_str(), min_column);
    if(max_column)      dataframe->addExistingColumn(FString("%s.stats.max",    sampler->rkey).c_str(), max_column);
    if(mean_column)     dataframe->addExistingColumn(FString("%s.stats.mean",   sampler->rkey).c_str(), mean_column);
    if(median_column)   dataframe->addExistingColumn(FString("%s.stats.median", sampler->rkey).c_str(), median_column);
    if(stdev_column)    dataframe->addExistingColumn(FString("%s.stats.stdev",  sampler->rkey).c_str(), stdev_column);
    if(mad_column)      dataframe->addExistingColumn(FString("%s.stats.mad",    sampler->rkey).c_str(), mad_column);
    if(scount_column)   dataframe->addExistingColumn(FString("%s.deriv.count",  sampler->rkey).c_str(), scount_column);
    if(slope_column)    dataframe->addExistingColumn(FString("%s.deriv.slope",  sampler->rkey).c_str(), slope_column);
    if(aspect_column)   dataframe->addExistingColumn(FString("%s.deriv.aspect", sampler->rkey).c_str(), aspect_column);

    // success
    return true;
}

/*----------------------------------------------------------------------------
 * populateColumns
 *----------------------------------------------------------------------------*/
bool DataFrameSampler::populateColumns (GeoDataFrame* dataframe, sampler_info_t* sampler)
{
    // create standard columns
    FieldColumn<double>* value_column = new FieldColumn<double>;
    FieldColumn<time8_t>* time_column = new FieldColumn<time8_t>;
    FieldColumn<uint64_t>* fileid_column = new FieldColumn<uint64_t>;

    // create flag column
    FieldColumn<uint32_t>* flags_column = NULL;
    if(sampler->robj->hasFlags()) flags_column = new FieldColumn<uint32_t>;

    // create band column
    FieldColumn<string>* band_column = NULL;
    if(sampler->robj->hasBands()) band_column = new FieldColumn<string>;

    // create zonal stat columns
    FieldColumn<uint32_t>* count_column = NULL;
    FieldColumn<double>* min_column = NULL;
    FieldColumn<double>* max_column = NULL;
    FieldColumn<double>* mean_column = NULL;
    FieldColumn<double>* median_column = NULL;
    FieldColumn<double>* stdev_column = NULL;
    FieldColumn<double>* mad_column = NULL;
    if(sampler->robj->hasZonalStats())
    {
        count_column = new FieldColumn<uint32_t>;
        min_column = new FieldColumn<double>;
        max_column = new FieldColumn<double>;
        mean_column = new FieldColumn<double>;
        median_column = new FieldColumn<double>;
        stdev_column = new FieldColumn<double>;
        mad_column = new FieldColumn<double>;
    }

    // create slope derivative columns
    FieldColumn<uint32_t>* scount_column = NULL;
    FieldColumn<double>* slope_column = NULL;
    FieldColumn<double>* aspect_column = NULL;
    if(sampler->robj->hasSpatialDerivs())
    {
        scount_column   = new FieldColumn<uint32_t>;
        slope_column    = new FieldColumn<double>;
        aspect_column   = new FieldColumn<double>;
    }

    // iterate over each sample in a vector of lists of samples
    for(int i = 0; i < sampler->samples.length(); i++)
    {
        sample_list_t* slist = sampler->samples[i];

        if(slist->length() > 0)
        {
            // populate core sample fields
            const RasterSample* sample = slist->get(0);
            value_column->append(sample->value);
            time_column->append(TimeLib::gps2systimeex(sample->time));
            fileid_column->append(sample->fileId);
            if(flags_column) flags_column->append(sample->flags);
            if(band_column) band_column->append(sample->bandName);

            // populate zonal stats fields
            if(sampler->robj->hasZonalStats())
            {
                assert(count_column);   count_column->append(sample->stats.count);
                assert(min_column);     min_column->append(sample->stats.min);
                assert(max_column);     max_column->append(sample->stats.max);
                assert(mean_column);    mean_column->append(sample->stats.mean);
                assert(median_column);  median_column->append(sample->stats.median);
                assert(stdev_column);   stdev_column->append(sample->stats.stdev);
                assert(mad_column);     mad_column->append(sample->stats.mad);
            }

            // populate slope derivative fields
            if(sampler->robj->hasSpatialDerivs())
            {
                assert(scount_column);  scount_column->append(sample->derivs.count);
                assert(slope_column);   slope_column->append(sample->derivs.slopeDeg);
                assert(aspect_column);  aspect_column->append(sample->derivs.aspectDeg);
            }
        }
        else
        {
            // populate core sample fields
            const string empty("na");
            value_column->append(std::numeric_limits<double>::quiet_NaN());
            time_column->append(TimeLib::gps2systimeex(0));
            fileid_column->append(0);
            if(flags_column) flags_column->append(0);
            if(band_column) band_column->append(empty);

            // populate zonal stats fields
            if(sampler->robj->hasZonalStats())
            {
                assert(count_column);   count_column->append(0);
                assert(min_column);     min_column->append(0);
                assert(max_column);     max_column->append(0);
                assert(mean_column);    mean_column->append(0);
                assert(median_column);  median_column->append(0);
                assert(stdev_column);   stdev_column->append(0);
                assert(mad_column);     mad_column->append(0);
            }

            // populate slope derivative fields
            if(sampler->robj->hasSpatialDerivs())
            {
                assert(scount_column);  scount_column->append(0);
                assert(slope_column);   slope_column->append(0);
                assert(aspect_column);  aspect_column->append(0);
            }
        }
    }

    // add new columns to dataframe
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column);
    dataframe->addExistingColumn(FString("%s.time_ns",  sampler->rkey).c_str(), time_column);
    dataframe->addExistingColumn(FString("%s.fileid",   sampler->rkey).c_str(), fileid_column);
    if(band_column)     dataframe->addExistingColumn(FString("%s.band",         sampler->rkey).c_str(), band_column);
    if(flags_column)    dataframe->addExistingColumn(FString("%s.flags",        sampler->rkey).c_str(), flags_column);
    if(count_column)    dataframe->addExistingColumn(FString("%s.stats.count",  sampler->rkey).c_str(), count_column);
    if(min_column)      dataframe->addExistingColumn(FString("%s.stats.min",    sampler->rkey).c_str(), min_column);
    if(max_column)      dataframe->addExistingColumn(FString("%s.stats.max",    sampler->rkey).c_str(), max_column);
    if(mean_column)     dataframe->addExistingColumn(FString("%s.stats.mean",   sampler->rkey).c_str(), mean_column);
    if(median_column)   dataframe->addExistingColumn(FString("%s.stats.median", sampler->rkey).c_str(), median_column);
    if(stdev_column)    dataframe->addExistingColumn(FString("%s.stats.stdev",  sampler->rkey).c_str(), stdev_column);
    if(mad_column)      dataframe->addExistingColumn(FString("%s.stats.mad",    sampler->rkey).c_str(), mad_column);
    if(scount_column)   dataframe->addExistingColumn(FString("%s.deriv.count",  sampler->rkey).c_str(), scount_column);
    if(slope_column)    dataframe->addExistingColumn(FString("%s.deriv.slope",  sampler->rkey).c_str(), slope_column);
    if(aspect_column)   dataframe->addExistingColumn(FString("%s.deriv.aspect", sampler->rkey).c_str(), aspect_column);

    // success
    return true;
}
