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

#include <cmath>
#include <limits>
#include <algorithm>

#include "OsApi.h"
#include "TimeLib.h"
#include "RasterObject.h"
#include "DataFrameSampler.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* DataFrameSampler::Runner::OBJECT_TYPE   = "DataFrameSamplerRunner";
const char* DataFrameSampler::Runner::LUA_META_NAME = "DataFrameSamplerRunner";
const struct luaL_Reg DataFrameSampler::Runner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * RUNNER METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - framesampler(parms)
 *----------------------------------------------------------------------------*/
int DataFrameSampler::Runner::luaCreate(lua_State* L)
{
    RequestParameters* _parms = NULL;
    try
    {
        _parms  = dynamic_cast<RequestParameters*>(getLuaObject(L, 1, RequestParameters::OBJECT_TYPE));
        return createLuaObject(L, new DataFrameSampler::Runner(L, _parms));
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
DataFrameSampler::Runner::Runner(lua_State* L, RequestParameters* _parms):
    FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
DataFrameSampler::Runner::~Runner(void)
{
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool DataFrameSampler::Runner::run (GeoDataFrame* dataframe)
{
    vector<point_info_t>    points;
    vector<sampler_info_t*> samplers;
    Dictionary<uint16_t>    band_index;

    try
    {
        // for each raster dataset that needs to be sampled
        buildSamplers(parms, samplers, band_index);

        // get and check crs
        const string& frame_crs = dataframe->getCRS();
        if(frame_crs.empty())
        {
            mlog(WARNING, "DataFrameSampler: incoming dataframe missing CRS");
        }

        // populate points vector
        populatePoints(points, dataframe, 0);

        // get samples for all user RasterObjects
        for(sampler_info_t* sampler: samplers)
        {
            sampler->robj->setCRS(frame_crs);

            // sample the rasters
            sampler->robj->getSamples(points, sampler->samples);

            // put samples into dataframe columns
            if(sampler->geoparms.force_single_sample.value != GeoFields::SINGLE_SAMPLE_NA)
            {
                populateColumns(sampler, band_index, dataframe, 0);
            }
            else
            {
                populateMultiColumns(sampler, band_index, dataframe, 0);
            }

            // add file id table metadata
            populateFileIds(sampler, dataframe);

            // release since not needed anymore
            sampler->samples.clear();
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sampling dataframe: %s", e.what());
    }

    // clean up samplers
    for(sampler_info_t* sampler: samplers)
    {
        sampler->robj->stopSampling();
        delete sampler->robj;
        delete sampler;
    }

    return true;
}

/******************************************************************************
 * BASE CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaSample(parms, dataframes)
 *----------------------------------------------------------------------------*/
int DataFrameSampler::luaSample (lua_State* L)
{
    RequestParameters*      parms = NULL;
    vector<GeoDataFrame*>   dataframes;
    vector<point_info_t>    points;
    vector<sampler_info_t*> samplers;
    Dictionary<uint16_t>    band_index;

    try
    {
        // get parameters
        parms = dynamic_cast<RequestParameters*>(LuaObject::getLuaObject(L, 1, RequestParameters::OBJECT_TYPE));

        // get table of dataframes
        lua_pushnil(L);
        while(lua_next(L, 2) != 0)
        {
            dataframes.push_back(dynamic_cast<GeoDataFrame*>(LuaObject::getLuaObject(L, -1, GeoDataFrame::OBJECT_TYPE)));
            lua_pop(L, 1);
        }

        // check empty
        if(dataframes.empty())
        {
            throw RunTimeException(INFO, RTE_STATUS, "no dataframes");
        }

        // for each raster dataset that needs to be sampled
        buildSamplers(parms, samplers, band_index);

        // get and check crs
        const string& frame_crs = dataframes[0]->getCRS();
        if(frame_crs.empty())
        {
            mlog(WARNING, "DataFrameSampler: incoming dataframe missing CRS");
        }

        // populate points vector
        long start_i = 0;
        for(GeoDataFrame* dataframe: dataframes)
        {
            start_i += populatePoints(points, dataframe, start_i);
        }

        // get samples for all user RasterObjects
        for(sampler_info_t* sampler: samplers)
        {
            sampler->robj->setCRS(frame_crs);

            // sample the rasters
            sampler->robj->getSamples(points, sampler->samples);

            // put samples into dataframe columns
            start_i = 0;
            for(GeoDataFrame* dataframe: dataframes)
            {
                if(sampler->geoparms.force_single_sample.value != GeoFields::SINGLE_SAMPLE_NA)
                {
                    start_i += populateColumns(sampler, band_index, dataframe, start_i);
                }
                else
                {
                    start_i += populateMultiColumns(sampler, band_index, dataframe, start_i);
                }
            }

            // add file id table metadata
            populateFileIds(sampler, dataframes[0]);

            // release since not needed anymore
            sampler->samples.clear();
        }

        // return success
        lua_pushboolean(L, true);
        lua_pushnil(L); // nil error message
    }
    catch(const RunTimeException& e)
    {
        const FString errmsg("Error sampling dataframe: %s", e.what());
        mlog(e.level(), "%s", errmsg.c_str());

        // return failure
        lua_pushboolean(L, false);
        lua_pushstring(L, errmsg.c_str());
    }

    // clean up samplers
    for(sampler_info_t* sampler: samplers)
    {
        sampler->robj->stopSampling();
        delete sampler->robj;
        delete sampler;
    }

    // clean up parms
    parms->releaseLuaObject();

    // clean up dataframes
    for(GeoDataFrame* dataframe: dataframes)
    {
        dataframe->releaseLuaObject();
    }

    // return back to lua
    return 2;
}
/*----------------------------------------------------------------------------
 * buildSamplers
 *----------------------------------------------------------------------------*/
void DataFrameSampler::buildSamplers (RequestParameters* parms, vector<sampler_info_t*>& samplers, Dictionary<uint16_t>& band_index)
{
    uint16_t index = 0;
    FieldMap<GeoFields>::entry_t geo_fields;
    const char* key = parms->samplers.fields.first(&geo_fields);
    while(key != NULL)
    {
        // create raster object
        RasterObject* robj = RasterObject::cppCreate(parms, key);
        if(!robj) throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create raster <%s>", key);

        // build sampler
        sampler_info_t* sampler = new sampler_info_t(key, robj, parms->samplers[key]);
        samplers.push_back(sampler);

        // create band index
        for(int i = 0; i < geo_fields.field->bands.length(); i++)
        {
            band_index.add(geo_fields.field->bands[i].c_str(), index);
            index++;
        }

        // go to next raster to sample
        key = parms->samplers.fields.next(NULL);
    }
}

/*----------------------------------------------------------------------------
 * populatePoints
 *----------------------------------------------------------------------------*/
long DataFrameSampler::populatePoints (vector<point_info_t>& points, GeoDataFrame* dataframe, long start_i)
{
    // get columns
    const FieldColumn<time8_t>*  t_column = dataframe->getTimeColumn();
    const FieldColumn<double>*  x_column = dataframe->getXColumn();
    const FieldColumn<double>*  y_column = dataframe->getYColumn();
    const FieldColumn<float>*   z_column = dataframe->getZColumn();

    // chech columns
    if(!x_column || !y_column)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Missing x and/or y columns (%d,%d)", x_column == NULL, y_column == NULL);
    }

    // initialize list of points
    for(long i = 0; i < dataframe->length(); i++)
    {
        points.emplace_back(point_info_t({{0.0, 0.0, 0.0}, 0}));
    }

    // populate x and y
    for(long i = 0; i < dataframe->length(); i++)
    {
        points[start_i + i].point3d.x = (*x_column)[i];
        points[start_i + i].point3d.y = (*y_column)[i];
    }

    // populate z (optionally)
    if(z_column)
    {
        for(long i = 0; i < dataframe->length(); i++)
        {
            points[start_i + i].point3d.z = static_cast<double>((*z_column)[i]);
        }
    }

    // populate time (optionally)
    if(t_column)
    {
        for(long i = 0; i < dataframe->length(); i++)
        {
            points[start_i + i].gps = TimeLib::sysex2gpstime((*t_column)[i]);
        }
    }

    // return number of points added
    return dataframe->length();
}

/*----------------------------------------------------------------------------
 * populateMultiColumns
 *----------------------------------------------------------------------------*/
long DataFrameSampler::populateMultiColumns (sampler_info_t* sampler, const Dictionary<uint16_t>& band_index, GeoDataFrame* dataframe, long start_i)
{
    // set ending index
    const long end_i = start_i + dataframe->length();

    // create standard columns
    FieldColumn<FieldList<double>>* value_column = new FieldColumn<FieldList<double>>(Field::NESTED_LIST);
    FieldColumn<FieldList<time8_t>>* time_column = new FieldColumn<FieldList<time8_t>>(Field::NESTED_LIST);
    FieldColumn<FieldList<uint64_t>>* fileid_column = new FieldColumn<FieldList<uint64_t>>(Field::NESTED_LIST);

    // create flag column
    FieldColumn<FieldList<uint32_t>>* flags_column = NULL;
    if(sampler->robj->hasFlags()) flags_column = new FieldColumn<FieldList<uint32_t>>(Field::NESTED_LIST);

    // create band column
    FieldColumn<FieldList<uint16_t>>* band_column = NULL;
    if(sampler->robj->hasBands()) band_column = new FieldColumn<FieldList<uint16_t>>(Field::NESTED_LIST);

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
    for(int i = start_i; i < end_i; i++)
    {
        sample_list_t* slist = sampler->samples[i];

        // populate core sample fields
        FieldList<double> value_list;
        FieldList<time8_t> time_list;
        FieldList<uint64_t> fileid_list;
        FieldList<uint32_t> flags_list;
        FieldList<uint16_t> band_list;
        for(int j = 0; j < slist->length(); j++)
        {
            const RasterSample* sample = slist->get(j);
            value_list.append(sample->value);
            time_list.append(TimeLib::gps2systimeex(sample->time));
            fileid_list.append(sample->fileId);
            if(flags_column) flags_list.append(sample->flags);
            if(band_column)
            {
                uint16_t index = 0xFFFF;
                band_index.find(sample->bandName.c_str(), &index);
                band_list.append(index);
            }
        }
        value_column->append(value_list);
        time_column->append(time_list);
        fileid_column->append(fileid_list);
        if(flags_column) flags_column->append(flags_list);
        if(band_column) band_column->append(band_list);

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
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column,   "Sampled value from the raster");
    dataframe->addExistingColumn(FString("%s.time_ns",  sampler->rkey).c_str(), time_column,    "Unix time (nanoseconds); the best time provided by the raster dataset for when the sampled value was measured");
    dataframe->addExistingColumn(FString("%s.fileid",   sampler->rkey).c_str(), fileid_column,  "A number used to identify the name of the file the sample value came from; this is used in conjunction with the file directory provided in the metadata of a GeoDataFrame");
    if(band_column)     dataframe->addExistingColumn(FString("%s.band",         sampler->rkey).c_str(), band_column,    "Source band of raster for the sample");
    if(flags_column)    dataframe->addExistingColumn(FString("%s.flags",        sampler->rkey).c_str(), flags_column,   "Any flags (if requested) that accompany the sampled data in the raster it was read from");
    if(count_column)    dataframe->addExistingColumn(FString("%s.stats.count",  sampler->rkey).c_str(), count_column,   "Number of pixels read to calculate sample value");
    if(min_column)      dataframe->addExistingColumn(FString("%s.stats.min",    sampler->rkey).c_str(), min_column,     "minimum pixel value of pixels that contributed to sample value");
    if(max_column)      dataframe->addExistingColumn(FString("%s.stats.max",    sampler->rkey).c_str(), max_column,     "Maximum pixel value of pixels that contributed to sample value");
    if(mean_column)     dataframe->addExistingColumn(FString("%s.stats.mean",   sampler->rkey).c_str(), mean_column,    "Average/mean pixel value of pixels that contributed to sample value");
    if(median_column)   dataframe->addExistingColumn(FString("%s.stats.median", sampler->rkey).c_str(), median_column,  "Average/median pixel value of pixels that contributed to sample value");
    if(stdev_column)    dataframe->addExistingColumn(FString("%s.stats.stdev",  sampler->rkey).c_str(), stdev_column,   "Standard deviation of pixel values of pixels that contributed to sample value");
    if(mad_column)      dataframe->addExistingColumn(FString("%s.stats.mad",    sampler->rkey).c_str(), mad_column,     "Median absolute deviation of pixel values of pixels that contributed to sample value");
    if(scount_column)   dataframe->addExistingColumn(FString("%s.deriv.count",  sampler->rkey).c_str(), scount_column,  "Number of pixels read to calculate slope and aspect");
    if(slope_column)    dataframe->addExistingColumn(FString("%s.deriv.slope",  sampler->rkey).c_str(), slope_column,   "The calculated slope at the location being sampled");
    if(aspect_column)   dataframe->addExistingColumn(FString("%s.deriv.aspect", sampler->rkey).c_str(), aspect_column,  "The calculated aspect at the location being sampled; the compass azimuth of the downslope direction in degrees clockwise from north, NaN where the surface is flat");

    // return number of values added
    return dataframe->length();
}

/*----------------------------------------------------------------------------
 * populateColumns
 *----------------------------------------------------------------------------*/
long DataFrameSampler::populateColumns (sampler_info_t* sampler, const Dictionary<uint16_t>& band_index, GeoDataFrame* dataframe, long start_i)
{
    // set ending index
    const long end_i = start_i + dataframe->length();

    // create standard columns
    FieldColumn<double>* value_column = new FieldColumn<double>;
    FieldColumn<time8_t>* time_column = new FieldColumn<time8_t>;
    FieldColumn<uint64_t>* fileid_column = new FieldColumn<uint64_t>;

    // create flag column
    FieldColumn<uint32_t>* flags_column = NULL;
    if(sampler->robj->hasFlags()) flags_column = new FieldColumn<uint32_t>;

    // create band column
    FieldColumn<uint16_t>* band_column = NULL;
    if(sampler->robj->hasBands()) band_column = new FieldColumn<uint16_t>;

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
    for(int i = start_i; i < end_i; i++)
    {
        sample_list_t* slist = sampler->samples[i];

        if( (slist->length() > 0) &&
            (sampler->geoparms.force_single_sample.value != GeoFields::SINGLE_SAMPLE_MEAN) &&
            (sampler->geoparms.force_single_sample.value != GeoFields::SINGLE_SAMPLE_MEDIAN) )
        {
            // select the sample
            const RasterSample* sample = slist->get(0); // default/initialize to first
            switch(sampler->geoparms.force_single_sample.value)
            {
                case GeoFields::SINGLE_SAMPLE_FIRST:
                {
                    for(int k = 0; k < slist->length(); k++)
                    {
                        if(std::isfinite(slist->get(k)->value))
                        {
                            sample = slist->get(k);
                            break; // don't look for any more
                        }
                    }
                    break;
                }

                case GeoFields::SINGLE_SAMPLE_LAST:
                {
                    for(int k = 0; k < slist->length(); k++)
                    {
                        if(std::isfinite(slist->get(k)->value))
                        {
                            sample = slist->get(k);
                            // keep looking to get the last one
                        }
                    }
                    break;
                }

                case GeoFields::SINGLE_SAMPLE_MIN:
                {
                    double min_val = std::numeric_limits<double>::max();
                    for(int k = 0; k < slist->length(); k++)
                    {
                        const double val = slist->get(k)->value;
                        if(std::isfinite(val) && (val <= min_val)) // equal comparison needed if initial value is NaN
                        {
                            min_val = val;
                            sample = slist->get(k);
                        }
                    }
                    break;
                }

                case GeoFields::SINGLE_SAMPLE_MAX:
                {
                    double max_val = std::numeric_limits<double>::lowest();
                    for(int k = 0; k < slist->length(); k++)
                    {
                        const double val = slist->get(k)->value;
                        if(std::isfinite(val) && (val >= max_val)) // equal comparison needed if initial value is NaN
                        {
                            max_val = val;
                            sample = slist->get(k);
                        }
                    }
                    break;
                }

                default: break; // see initial value
            }

            // populate core sample fields
            value_column->append(sample->value);
            time_column->append(TimeLib::gps2systimeex(sample->time));
            fileid_column->append(sample->fileId);
            if(flags_column) flags_column->append(sample->flags);
            if(band_column)
            {
                uint16_t index = 0xFFFF;
                band_index.find(sample->bandName.c_str(), &index);
                band_column->append(index);
            }

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
            // populate value
            if((slist->length() > 0) && (sampler->geoparms.force_single_sample.value == GeoFields::SINGLE_SAMPLE_MEAN))
            {
                double mean_value = 0.0;
                double mean_cnt = 0;
                for(int k = 0; k < slist->length(); k++)
                {
                    const RasterSample* sample = slist->get(k);
                    if(std::isfinite(sample->value))
                    {
                        mean_value += sample->value;
                        mean_cnt += 1;
                    }
                }
                if(mean_cnt > 0) value_column->append(mean_value / mean_cnt);
                else value_column->append(std::numeric_limits<double>::quiet_NaN());
            }
            else if((slist->length() > 0) && (sampler->geoparms.force_single_sample.value == GeoFields::SINGLE_SAMPLE_MEDIAN))
            {
                vector<double> values;
                for(int k = 0; k < slist->length(); k++)
                {
                    const RasterSample* sample = slist->get(k);
                    if(std::isfinite(sample->value))
                    {
                        values.push_back(sample->value);
                    }
                }
                if(!values.empty())
                {
                    const size_t n = values.size() / 2;
                    std::nth_element(values.begin(), values.begin() + n, values.end());
                    value_column->append(values[n]);
                }
                else
                {
                    value_column->append(std::numeric_limits<double>::quiet_NaN());
                }
            }
            else
            {
                value_column->append(std::numeric_limits<double>::quiet_NaN());
            }

            // populate remaining core sample fields
            time_column->append(TimeLib::gps2systimeex(0));
            fileid_column->append(0);
            if(flags_column) flags_column->append(0);
            if(band_column) band_column->append(0xFFFF);

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
    dataframe->addExistingColumn(FString("%s.value",    sampler->rkey).c_str(), value_column,   "Sampled value from the raster");
    dataframe->addExistingColumn(FString("%s.time_ns",  sampler->rkey).c_str(), time_column,    "Unix time (nanoseconds); the best time provided by the raster dataset for when the sampled value was measured");
    dataframe->addExistingColumn(FString("%s.fileid",   sampler->rkey).c_str(), fileid_column,  "A number used to identify the name of the file the sample value came from; this is used in conjunction with the file directory provided in the metadata of a GeoDataFrame");
    if(band_column)     dataframe->addExistingColumn(FString("%s.band",         sampler->rkey).c_str(), band_column,    "Source band of raster for the sample");
    if(flags_column)    dataframe->addExistingColumn(FString("%s.flags",        sampler->rkey).c_str(), flags_column,   "Any flags (if requested) that accompany the sampled data in the raster it was read from");
    if(count_column)    dataframe->addExistingColumn(FString("%s.stats.count",  sampler->rkey).c_str(), count_column,   "Number of pixels read to calculate sample value");
    if(min_column)      dataframe->addExistingColumn(FString("%s.stats.min",    sampler->rkey).c_str(), min_column,     "minimum pixel value of pixels that contributed to sample value");
    if(max_column)      dataframe->addExistingColumn(FString("%s.stats.max",    sampler->rkey).c_str(), max_column,     "Maximum pixel value of pixels that contributed to sample value");
    if(mean_column)     dataframe->addExistingColumn(FString("%s.stats.mean",   sampler->rkey).c_str(), mean_column,    "Average/mean pixel value of pixels that contributed to sample value");
    if(median_column)   dataframe->addExistingColumn(FString("%s.stats.median", sampler->rkey).c_str(), median_column,  "Average/median pixel value of pixels that contributed to sample value");
    if(stdev_column)    dataframe->addExistingColumn(FString("%s.stats.stdev",  sampler->rkey).c_str(), stdev_column,   "Standard deviation of pixel values of pixels that contributed to sample value");
    if(mad_column)      dataframe->addExistingColumn(FString("%s.stats.mad",    sampler->rkey).c_str(), mad_column,     "Median absolute deviation of pixel values of pixels that contributed to sample value");
    if(scount_column)   dataframe->addExistingColumn(FString("%s.deriv.count",  sampler->rkey).c_str(), scount_column,  "Number of pixels read to calculate slope and aspect");
    if(slope_column)    dataframe->addExistingColumn(FString("%s.deriv.slope",  sampler->rkey).c_str(), slope_column,   "The calculated slope at the location being sampled");
    if(aspect_column)   dataframe->addExistingColumn(FString("%s.deriv.aspect", sampler->rkey).c_str(), aspect_column,  "The calculated aspect at the location being sampled; the compass azimuth of the downslope direction in degrees clockwise from north, NaN where the surface is flat");

    // return number of values added
    return dataframe->length();
}

/*----------------------------------------------------------------------------
 * populateFileIds
 *----------------------------------------------------------------------------*/
void DataFrameSampler::populateFileIds (sampler_info_t* sampler, GeoDataFrame* dataframe)
{
    FieldMap<Field> file_id_table;
    const std::set<uint64_t>& file_ids = sampler->robj->fileDictGetSampleIds();
    for(std::set<uint64_t>::const_iterator file_id_iter = file_ids.begin(); file_id_iter != file_ids.end(); file_id_iter++)
    {
        // pull out file_id and file_name from file id dictionary
        const uint64_t file_id = *file_id_iter;
        const char* file_name = sampler->robj->fileDictGet(file_id);

        // build string(file_id) and field element(file_name)
        const FString key("%lu", file_id);
        FieldElement<string>* field = new FieldElement<string>(file_name);

        // build dictionary <raster>[<file_id>] = <file_name>
        if(!file_id_table.add(key.c_str(), field, NULL, true))
        {
            delete field;
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to add metadata field <%s> to <%s>", key.c_str(), sampler->rkey);
        }
    }

    // build json file id table entry
    const string value = file_id_table.toJson();
    FieldElement<string>* field = new FieldElement<string>(value);

    // add file id table metadata entry for raster
    const FString key("%s.%s", GeoFields::PARMS, sampler->rkey);
    if(!dataframe->addMetaData(key.c_str(), field, StringLib::duplicate("File ID table"), true))
    {
        delete field;
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to file id table for <%s> to dataframe metadata", key.c_str());

    }
}
