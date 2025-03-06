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
    const char* key = parms->samplers.values.first(NULL);
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
        key = parms->samplers.values.next(NULL);
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

    // populate points vector
    if(!populatePoints(dataframe))
    {
        mlog(CRITICAL, "Failed to populate points for sampling");
        return false;
    }

    // get samples for all user RasterObjects
    for(sampler_info_t* sampler: samplers)
    {
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
    const FieldColumn<double>*  z_column = dataframe->getZColumn();

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
            points[i].point3d.z = (*z_column)[i];
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
    FieldColumn<FieldList<double>>* time_column = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
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

    // iterate over each sample in a vector of lists of samples
    for(int i = 0; i < sampler->samples.length(); i++)
    {
        FieldList<double> value_list;
        FieldList<double> time_list;
        FieldList<uint64_t> fileid_list;
        FieldList<uint32_t> flags_list;
        FieldList<string> band_list;
        FieldList<uint32_t> count_list;
        FieldList<double> min_list;
        FieldList<double> max_list;
        FieldList<double> mean_list;
        FieldList<double> median_list;
        FieldList<double> stdev_list;
        FieldList<double> mad_list;
        sample_list_t* slist = sampler->samples[i];
        for(int j = 0; j < slist->length(); j++)
        {
            const RasterSample* sample = slist->get(j);
            value_list.append(sample->value);
            time_list.append(sample->time);
            fileid_list.append(sample->fileId);
            if(flags_column)    flags_list.append(sample->flags);
            if(band_column)     band_list.append(sample->bandName);
            if(count_column)    count_list.append(sample->stats.count);
            if(min_column)      min_list.append(sample->stats.min);
            if(max_column)      max_list.append(sample->stats.max);
            if(mean_column)     mean_list.append(sample->stats.mean);
            if(median_column)   median_list.append(sample->stats.median);
            if(stdev_column)    stdev_list.append(sample->stats.stdev);
            if(mad_column)      mad_list.append(sample->stats.mad);
        }
        value_column->append(value_list);
        time_column->append(time_list);
        fileid_column->append(fileid_list);
        if(flags_column)    flags_column->append(flags_list);
        if(band_column)     band_column->append(band_list);
        if(count_column)    count_column->append(count_list);
        if(min_column)      min_column->append(min_list);
        if(max_column)      max_column->append(max_list);
        if(mean_column)     mean_column->append(mean_list);
        if(median_column)   median_column->append(median_list);
        if(stdev_column)    stdev_column->append(stdev_list);
        if(mad_column)      mad_column->append(mad_list);
    }

    // add new columns to dataframe
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column);
    dataframe->addExistingColumn(FString("%s.time",     sampler->rkey).c_str(), time_column);
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
    FieldColumn<double>* time_column = new FieldColumn<double>;
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

    // iterate over each sample in a vector of lists of samples
    for(int i = 0; i < sampler->samples.length(); i++)
    {
        sample_list_t* slist = sampler->samples[i];
        if(slist->length() > 0)
        {
            const RasterSample* sample = slist->get(0);
            value_column->append(sample->value);
            time_column->append(sample->time);
            fileid_column->append(sample->fileId);
            if(flags_column)    flags_column->append(sample->flags);
            if(band_column)     band_column->append(sample->bandName);
            if(count_column)    count_column->append(sample->stats.count);
            if(min_column)      min_column->append(sample->stats.min);
            if(max_column)      max_column->append(sample->stats.max);
            if(mean_column)     mean_column->append(sample->stats.mean);
            if(median_column)   median_column->append(sample->stats.median);
            if(stdev_column)    stdev_column->append(sample->stats.stdev);
            if(mad_column)      mad_column->append(sample->stats.mad);
        }
        else
        {
            const string empty("na");
            value_column->append(std::numeric_limits<double>::quiet_NaN());
            time_column->append(0);
            fileid_column->append(0);
            if(flags_column)    flags_column->append(0);
            if(band_column)     band_column->append(empty);
            if(count_column)    count_column->append(0);
            if(min_column)      min_column->append(0);
            if(max_column)      max_column->append(0);
            if(mean_column)     mean_column->append(0);
            if(median_column)   median_column->append(0);
            if(stdev_column)    stdev_column->append(0);
            if(mad_column)      mad_column->append(0);
        }
    }

    // add new columns to dataframe
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column);
    dataframe->addExistingColumn(FString("%s.time",     sampler->rkey).c_str(), time_column);
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

    // success
    return true;
}
