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

#include <uuid/uuid.h>

#include "estimate_kd.h"
#include "estimate_surface_roughness.h"

#include "Atl24Writer.h"
#include "PluginFields.h"
#include "OsApi.h"
#include "EventLib.h"
#include "List.h"
#include "GeoDataFrame.h"
#include "FieldColumn.h"
#include "TimeLib.h"
#include "BathyDataFrame.h"
#include "Icesat2Parameters.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl24Writer::OBJECT_TYPE = "Atl24Writer";
const char* Atl24Writer::LUA_META_NAME = "Atl24Writer";
const struct luaL_Reg Atl24Writer::LUA_META_TABLE[] = {
    {"write",       luaWriteFile},
    {NULL,          NULL}
};

const char* Atl24Writer::BEAMS[NUM_BEAMS] = {"gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"};

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

static void goto_parent(List<HdfLib::dataset_t>& datasets)
{
    HdfLib::dataset_t parent = {NULL, HdfLib::PARENT, RecordObject::INVALID_FIELD, NULL, 0} ;
    datasets.add(parent);
}

static void add_group(List<HdfLib::dataset_t>& datasets, const char* name)
{
    HdfLib::dataset_t group = {name, HdfLib::GROUP, RecordObject::INVALID_FIELD, NULL, 0} ;
    datasets.add(group);
}

static void add_variable(List<HdfLib::dataset_t>& datasets, const char* name, const Field* field)
{
    long size = field->length() * field->getTypeSize();
    uint8_t* buffer = new uint8_t[size];
    field->serialize(buffer, size);
    HdfLib::dataset_t variable = {name, HdfLib::VARIABLE, static_cast<RecordObject::fieldType_t>(field->getEncodedType()), buffer, size};
    datasets.add(variable);
}

static void add_scalar(List<HdfLib::dataset_t>& datasets, const char* name, const Field* field)
{
    long size = field->length() * field->getTypeSize();
    uint8_t* buffer = new uint8_t[size];
    field->serialize(buffer, size);
    HdfLib::dataset_t scalar = {name, HdfLib::SCALAR, static_cast<RecordObject::fieldType_t>(field->getEncodedType()), buffer, size};
    datasets.add(scalar);
}

static void add_attribute(List<HdfLib::dataset_t>& datasets, const char* name, const char* value)
{
    long size = StringLib::size(value) + 1;
    char* buffer = StringLib::duplicate(value);
    HdfLib::dataset_t attribute = {name, HdfLib::ATTRIBUTE, RecordObject::STRING, reinterpret_cast<uint8_t*>(buffer), size};
    datasets.add(attribute);
}

static void add_attribute_double(List<HdfLib::dataset_t>& datasets, const char* name, const double value)
{
    double* buffer = new double[1];
    buffer[0] = value;
    HdfLib::dataset_t attribute = {name, HdfLib::ATTRIBUTE, RecordObject::DOUBLE, reinterpret_cast<uint8_t*>(buffer), sizeof(value)};
    datasets.add(attribute);
}

static void add_attribute_float(List<HdfLib::dataset_t>& datasets, const char* name, const float value)
{
    float* buffer = new float[1];
    buffer[0] = value;
    HdfLib::dataset_t attribute = {name, HdfLib::ATTRIBUTE, RecordObject::FLOAT, reinterpret_cast<uint8_t*>(buffer), sizeof(value)};
    datasets.add(attribute);
}

#if 0
static void add_attribute_int32(List<HdfLib::dataset_t>& datasets, const char* name, const int32_t value)
{
    int32_t* buffer = new int32_t[1];
    buffer[0] = value;
    HdfLib::dataset_t attribute = {name, HdfLib::ATTRIBUTE, RecordObject::INT32, reinterpret_cast<uint8_t*>(buffer), sizeof(value)};
    datasets.add(attribute);
}

static void add_attribute_int8(List<HdfLib::dataset_t>& datasets, const char* name, const int8_t value)
{
    int8_t* buffer = new int8_t[1];
    buffer[0] = value;
    HdfLib::dataset_t attribute = {name, HdfLib::ATTRIBUTE, RecordObject::INT32, reinterpret_cast<uint8_t*>(buffer), sizeof(value)};
    datasets.add(attribute);
}
#endif

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl24Writer::init (void)
{
    // do nothing
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<table of beams>)
 *----------------------------------------------------------------------------*/
int Atl24Writer::luaCreate (lua_State* L)
{
    Icesat2Parameters* _parms = NULL;
    BathyDataFrame* _dataframes[NUM_BEAMS] = {NULL, NULL, NULL, NULL, NULL, NULL};
    Atl03Granule* _granule = NULL;

    try
    {
        int parms_index = 1;
        int dataframe_table_index = 2;
        int granule_index = 3;
        int release_index = 4;

        /* Get Parameters */
        _parms = dynamic_cast<Icesat2Parameters*>(getLuaObject(L, parms_index, Icesat2Parameters::OBJECT_TYPE));

        /* Get DataFrames */
        if(lua_istable(L, dataframe_table_index))
        {
            for(int i = 0; i < NUM_BEAMS; i++)
            {
                lua_getfield(L, dataframe_table_index, BEAMS[i]);
                if(!lua_isnil(L, -1))
                {
                    _dataframes[i] = dynamic_cast<BathyDataFrame*>(getLuaObject(L, -1, GeoDataFrame::OBJECT_TYPE));
                }
                lua_pop(L, 1);
            }
        }

        /* Get Granule */
        _granule = dynamic_cast<Atl03Granule*>(getLuaObject(L, granule_index, Atl03Granule::OBJECT_TYPE));

        /* Get Release Number */
        const char* _release = getLuaString(L, release_index);

        /* Return Dispatch Object */
        return createLuaObject(L, new Atl24Writer(L, _parms, _dataframes, _granule, _release));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        for(int i = 0; i < NUM_BEAMS; i++)
        {
            if(_dataframes[i]) _dataframes[i]->releaseLuaObject();
        }
        if(_granule) _granule->releaseLuaObject();
        if(_parms) _parms->releaseLuaObject();
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl24Writer::Atl24Writer(lua_State* L, Icesat2Parameters* _parms, BathyDataFrame** _dataframes, Atl03Granule* _granule, const char* _release):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    release(FString("0%s", _release).c_str()),
    parms(_parms),
    granule(_granule)
{
    for(int i = 0; i < NUM_BEAMS; i++)
    {
        dataframes[i] = _dataframes[i];
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl24Writer::~Atl24Writer(void)
{
    if(parms)
    {
        parms->releaseLuaObject();
    }

    for(int i = 0; i < NUM_BEAMS; i++)
    {
        if(dataframes[i])
        {
            dataframes[i]->releaseLuaObject();
        }
    }

    if(granule)
    {
        granule->releaseLuaObject();
    }
}

/*----------------------------------------------------------------------------
 * writeFile
 *----------------------------------------------------------------------------*/
int Atl24Writer::luaWriteFile(lua_State* L)
{
    bool status;
    List<HdfLib::dataset_t> datasets;

    try
    {
        /* Get Self */
        Atl24Writer* lua_obj = dynamic_cast<Atl24Writer*>(getLuaSelf(L, 1));
        Icesat2Parameters* parms = lua_obj->parms;
        Atl03Granule& granule = *lua_obj->granule;

        /* Get Filename */
        const char* filename = getLuaString(L, 2);

        /* Get Metadata */
        PluginFields pluginFields;
        FieldElement<string> atl24_metadata(pluginFields.toJson());
        FieldElement<string> rqst_metadata(parms->toJson());

        /**********************/
        /* Create Beam Groups */
        /**********************/
        BathyDataFrame* last_df = NULL;
        for(int i = 0; i < NUM_BEAMS; i++)
        {
            /* Get and Check DataFrame for Beam */
            BathyDataFrame* df = lua_obj->dataframes[i];
            if(!df || df->time_ns.length() <= 0) continue;
            last_df = df;

            /* Create Beam Group */
            add_group(datasets, BEAMS[i]);

            /* Create Variable - class_ph */
            FieldColumn<int8_t>* class_ph = reinterpret_cast<FieldColumn<int8_t>*>(df->getColumn("class_ph"));
            add_variable(datasets, "class_ph", class_ph);
            add_attribute(datasets, "contentType", "modelResults");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "0 - unclassified, 1 - other, 2 - ground, 40 - bathymetry, 41 - sea surface");
            add_attribute(datasets, "long_name", "Photon classification");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "scalar");
            goto_parent(datasets);

            /* Create Variable - confidence */
            FieldColumn<float>* confidence = reinterpret_cast<FieldColumn<float>*>(df->getColumn("confidence"));
            add_variable(datasets, "confidence", confidence);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "ensemble confidence score from 0.0 to 1.0 where larger numbers represent higher confidence in classification");
            add_attribute(datasets, "long_name", "Ensemble confidence");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "scalar");
            goto_parent(datasets);

            /* Create Variable - delta_time */
            FieldColumn<double> delta_time;
            for(long j = 0; j < df->time_ns.length(); j++)
            {
                static const double ATLAS_LEAP_SECONDS = 18; // optimization based on the time period of ATLAS data at the time of ATL24 generation (2025)
                double value = (df->time_ns[j].nanoseconds / 1000000000.0) - (Icesat2Parameters::ATLAS_SDP_EPOCH_GPS + TimeLib::GPS_EPOCH_START - ATLAS_LEAP_SECONDS);
                delta_time.append(value);
            }
            add_variable(datasets, "delta_time", &delta_time);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "lat_ph lon_ph");
            add_attribute(datasets, "description", "The transmit time of a given photon, measured in seconds from the ATLAS Standard Data Product Epoch. Note that multiple received photons associated with a single transmit pulse will have the same delta_time. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.");
            add_attribute(datasets, "long_name", "Elapsed GPS seconds");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "seconds since 2018-01-01");
            goto_parent(datasets);

            /* Create Variable - ellipse_h */
            add_variable(datasets, "ellipse_h", &df->ellipse_h);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Height of each received photon, relative to the WGS-84 ellipsoid including refraction correction. Note neither the geoid, ocean tide nor the dynamic atmosphere (DAC) corrections are applied to the ellipsoidal heights.");
            add_attribute(datasets, "long_name", "Photon WGS84 height");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - index_ph */
            add_variable(datasets, "index_ph", &df->index_ph);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "0-based index of the photon in the ATL03 heights group");
            add_attribute(datasets, "long_name", "Photon index");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "scalar");
            goto_parent(datasets);

            /* Create Variable - index_seg */
            add_variable(datasets, "index_seg", &df->index_seg);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "0-based index of the photon in the ATL03 geolocation group");
            add_attribute(datasets, "long_name", "Segment index");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "scalar");
            goto_parent(datasets);

            /* Create Variable - index_seg */
            add_variable(datasets, "segment_id", &df->segment_id);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "ATL03 segment id of the photon");
            add_attribute(datasets, "long_name", "Segment ID");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "scalar");
            goto_parent(datasets);

            /* Create Variable - lat_ph */
            add_variable(datasets, "lat_ph", &df->lat_ph);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lon_ph");
            add_attribute(datasets, "description", "Latitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.");
            add_attribute(datasets, "long_name", "Latitude");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "degrees_north");
            add_attribute(datasets, "standard_name", "latitude");
            add_attribute_double(datasets, "valid_max", 90.0);
            add_attribute_double(datasets, "valid_min", -90.0);
            goto_parent(datasets);

            /* Create Variable - lon_ph */
            add_variable(datasets, "lon_ph", &df->lon_ph);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph");
            add_attribute(datasets, "description", "Longitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.");
            add_attribute(datasets, "long_name", "Longitude");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "degrees_east");
            add_attribute(datasets, "standard_name", "longitude");
            add_attribute_double(datasets, "valid_max", 180.0);
            add_attribute_double(datasets, "valid_min", -180.0);
            goto_parent(datasets);

            /* Create Variable - night_flag */
            FieldColumn<int8_t> night_flag;
            for(long i = 0; i < df->processing_flags.length(); i++)
            {
                night_flag.append(static_cast<int8_t>((df->processing_flags[i] & BathyParameters::NIGHT_FLAG) != 0));
            }
            add_variable(datasets, "night_flag", &night_flag);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "The solar elevation was less than 5 degrees at the time and location of the photon");
            add_attribute(datasets, "long_name", "Night flag");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "boolean");
            add_attribute(datasets, "flag_meanings", "false, true");
            add_attribute(datasets, "flag_values", "0, 1");
            goto_parent(datasets);

            /* Create Variable - ortho_h */
            add_variable(datasets, "ortho_h", &df->geoid_corr_h);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Height of each received photon, relative to the geoid.");
            add_attribute(datasets, "long_name", "Orthometric height");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - sigma_thu */
            FieldColumn<float>* sigma_thu = reinterpret_cast<FieldColumn<float>*>(df->getColumn("sigma_thu"));
            add_variable(datasets, "sigma_thu", sigma_thu);
            add_attribute(datasets, "contentType", "physicalMeasurement");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "The combination of the aerial and subaqueous horizontal uncertainty for each received photon");
            add_attribute(datasets, "long_name", "Total horizontal uncertainty");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - sigma_tvu */
            FieldColumn<float>* sigma_tvu = reinterpret_cast<FieldColumn<float>*>(df->getColumn("sigma_tvu"));
            add_variable(datasets, "sigma_tvu", sigma_tvu);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "The combination of the aerial and subaqueous vertical uncertainty for each received photon");
            add_attribute(datasets, "long_name", "Total vertical uncertainty");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - surface_h */
            FieldColumn<float>* surface_h = reinterpret_cast<FieldColumn<float>*>(df->getColumn("surface_h"));
            add_variable(datasets, "surface_h", surface_h);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "The geoid corrected height of the sea surface at the detected photon");
            add_attribute(datasets, "long_name", "Sea surface orthometric height");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - x_atc */
            add_variable(datasets, "x_atc", &df->x_atc);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Along-track distance in a segment projected to the ellipsoid of the received photon, based on the Along-Track Segment algorithm.  Total along track distance can be found by adding this value to the sum of segment lengths measured from the start of the most recent reference groundtrack.");
            add_attribute(datasets, "long_name", "Distance from equator crossing");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - y_atc */
            add_variable(datasets, "y_atc", &df->y_atc);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Across-track distance projected to the ellipsoid of the received photon from the reference ground track.  This is based on the Along-Track Segment algorithm described in Section 3.1 of the ATBD.");
            add_attribute(datasets, "long_name", "Distance off RGT");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            goto_parent(datasets);

            /* Create Variable - kd */
            FieldColumn<float>* kd = reinterpret_cast<FieldColumn<float>*>(df->getColumn("kd"));
            const ATL24::estimate_kd::Params kd_params;
            add_variable(datasets, "kd", kd);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Turbidity of water column calculated using only ICESat-2 photons");
            add_attribute(datasets, "long_name", "Turbidity");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            add_attribute_float(datasets, "_FillValue", kd_params.invalid_value);
            goto_parent(datasets);

            /* Create Variable - surface_roughness */
            FieldColumn<float>* surface_roughness = reinterpret_cast<FieldColumn<float>*>(df->getColumn("surface_roughness"));
            const ATL24::estimate_kd::Params surface_roughness_params;
            add_variable(datasets, "surface_roughness", surface_roughness);
            add_attribute(datasets, "contentType", "modelResult");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Measure of wave heights and proxy for wind speed in uncertainty calculation");
            add_attribute(datasets, "long_name", "Surface Roughness");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "meters");
            add_attribute_float(datasets, "_FillValue", surface_roughness_params.invalid_value);
            goto_parent(datasets);

            /* Create Variable - quality_ph */
            FieldColumn<int8_t>* quality_ph = reinterpret_cast<FieldColumn<int8_t>*>(df->getColumn("quality_ph"));
            add_variable(datasets, "quality_ph", quality_ph);
            add_attribute(datasets, "contentType", "qualityInformation");
            add_attribute(datasets, "coordinates", "delta_time lat_ph lon_ph");
            add_attribute(datasets, "description", "Indicates the quality of the associated photon. A value of 0 means nominal. See ATL03 ATBD: ATLAS Saturation for description of non-nominal values.");
            add_attribute(datasets, "long_name", "Photon Quality");
            add_attribute(datasets, "source", "ATL03");
            add_attribute(datasets, "units", "1");
            goto_parent(datasets);

            /* Go Back to Parent Group */
            goto_parent(datasets);
        }

        /* Check For At Least One Beam */
        if(last_df == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Attempted to write ATL24 file with no beams");

        /*******************************/
        /* Create Ancillary Data Group */
        /*******************************/
        add_group(datasets, "ancillary_data");

        /* Create Variable - atlas_sdp_gps_epoch */
        add_scalar(datasets, "atlas_sdp_gps_epoch", &granule["atlas_sdp_gps_epoch"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS Standard Data Product (SDP) epoch (2018-01-01:T00.00.00.000000 UTC). Add this value to delta time parameters to compute full gps_seconds (relative to the GPS epoch) for each data point.");
        add_attribute(datasets, "long_name", "ATLAS Epoch Offset");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "seconds since 1980-01-06T00:00:00.000000Z");
        goto_parent(datasets);

        /* Create Variable - data_end_utc */
        add_scalar(datasets, "data_end_utc", &granule["data_start_utc"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "UTC (in CCSDS-A format) of the last data point within the granule.");
        add_attribute(datasets, "long_name", "End UTC Time of Granule (CCSDS-A, Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - data_start_utc */
        add_scalar(datasets, "data_start_utc", &granule["data_start_utc"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "UTC (in CCSDS-A format) of the first data point within the granule.");
        add_attribute(datasets, "long_name", "Start UTC Time of Granule (CCSDS-A, Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - end_cycle */
        add_scalar(datasets, "end_cycle", last_df->getMetaData("cycle"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The ending cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.");
        add_attribute(datasets, "long_name", "Ending Cycle");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - end_delta_time */
        add_scalar(datasets, "end_delta_time", &granule["end_delta_time"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Number of GPS seconds since the ATLAS SDP epoch at the last data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.");
        add_attribute(datasets, "long_name", "ATLAS End Time (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "seconds since 2018-01-01");
        add_attribute(datasets, "standard_name", "time");
        goto_parent(datasets);

        /* Create Variable - end_geoseg */
        add_scalar(datasets, "end_geoseg", &granule["end_geoseg"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The ending geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.");
        add_attribute(datasets, "long_name", "Ending Geolocation Segment");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - end_gpssow */
        add_scalar(datasets, "end_gpssow", &granule["end_gpssow"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "GPS seconds-of-week of the last data point in the granule.");
        add_attribute(datasets, "long_name", "Ending GPS SOW of Granule (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "seconds");
        goto_parent(datasets);

        /* Create Variable - end_gpsweek */
        add_scalar(datasets, "end_gpsweek", &granule["end_gpsweek"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "GPS week number of the last data point in the granule.");
        add_attribute(datasets, "long_name", "Ending GPSWeek of Granule (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "weeks from 1980-01-06");
        goto_parent(datasets);

        /* Create Variable - end_orbit */
        add_scalar(datasets, "end_orbit", &granule["orbit_number"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The ending orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.");
        add_attribute(datasets, "long_name", "Ending Orbit Number");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - end_region */
        add_scalar(datasets, "end_region", last_df->getMetaData("region"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The ending product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.");
        add_attribute(datasets, "long_name", "Ending Region");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - end_rgt */
        add_scalar(datasets, "end_rgt", last_df->getMetaData("rgt"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The ending reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.");
        add_attribute(datasets, "long_name", "Ending Reference Groundtrack");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - granule_end_utc */
        add_scalar(datasets, "granule_end_utc", &granule["granule_end_utc"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Requested end time (in UTC CCSDS-A) of this granule.");
        add_attribute(datasets, "long_name", "End UTC Time of Granule (CCSDS-A, Requested)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - granule_start_utc */
        add_scalar(datasets, "granule_start_utc", &granule["granule_start_utc"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Requested start time (in UTC CCSDS-A) of this granule.");
        add_attribute(datasets, "long_name", "Start UTC Time of Granule (CCSDS-A, Requested)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - release */
        add_scalar(datasets, "release", &lua_obj->release);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Release number of the granule. The release number is incremented when the software or ancillary data used to create the granule has been changed.");
        add_attribute(datasets, "long_name", "Release Number");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - resource */
        add_scalar(datasets, "resource", last_df->getMetaData("granule"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "ATL03 granule used to produce this granule");
        add_attribute(datasets, "long_name", "ATL03 Resource");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - sliderule_version */
        add_scalar(datasets, "sliderule_version", &parms->slideruleVersion);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Version of SlideRule software used to generate this granule");
        add_attribute(datasets, "long_name", "SlideRule Version");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - sliderule_commit */
        add_scalar(datasets, "sliderule_commit", &parms->buildInformation);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule software used to generate this granule");
        add_attribute(datasets, "long_name", "SlideRule Commit");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - sliderule_environment */
        add_scalar(datasets, "sliderule_environment", &parms->environmentVersion);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule environment used to generate this granule");
        add_attribute(datasets, "long_name", "SlideRule Environment");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - start_cycle */
        add_scalar(datasets, "start_cycle", last_df->getMetaData("cycle"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The starting cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.");
        add_attribute(datasets, "long_name", "Starting Cycle");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - start_delta_time */
        add_scalar(datasets, "start_delta_time", &granule["start_delta_time"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Number of GPS seconds since the ATLAS SDP epoch at the first data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.");
        add_attribute(datasets, "long_name", "ATLAS Start Time (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "seconds since 2018-01-01");
        goto_parent(datasets);

        /* Create Variable - start_geoseg */
        add_scalar(datasets, "start_geoseg", &granule["start_geoseg"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The starting geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.");
        add_attribute(datasets, "long_name", "Starting Geolocation Segment");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - start_gpssow */
        add_scalar(datasets, "start_gpssow", &granule["start_gpssow"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "GPS seconds-of-week of the first data point in the granule.");
        add_attribute(datasets, "long_name", "Start GPS SOW of Granule (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "seconds");
        goto_parent(datasets);

        /* Create Variable - start_gpsweek */
        add_scalar(datasets, "start_gpsweek", &granule["start_gpsweek"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "GPS week number of the first data point in the granule.");
        add_attribute(datasets, "long_name", "Start GPSWeek of Granule (Actual)");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "weeks from 1980-01-06");
        goto_parent(datasets);

        /* Create Variable - start_orbit */
        add_scalar(datasets, "start_orbit", &granule["orbit_number"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The starting orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.");
        add_attribute(datasets, "long_name", "Starting Orbit Number");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - start_region */
        add_scalar(datasets, "start_region", last_df->getMetaData("region"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The starting product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.");
        add_attribute(datasets, "long_name", "Starting Region");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - start_rgt */
        add_scalar(datasets, "start_rgt", last_df->getMetaData("rgt"));
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "The starting reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.");
        add_attribute(datasets, "long_name", "Starting Reference Groundtrack");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - version */
        add_scalar(datasets, "version", &granule["version"]);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "Version number of this granule within the release. It is a sequential number corresponding to the number of times the granule has been reprocessed for the current release.");
        add_attribute(datasets, "long_name", "Version");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Go Back to Parent Group */
        goto_parent(datasets);

        /***************************/
        /* Create Orbit Info Group */
        /***************************/
        add_group(datasets, "orbit_info");

        /* Create Variable - crossing_time */
        add_scalar(datasets, "crossing_time", &granule["crossing_time"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "The time, in seconds since the ATLAS SDP GPS Epoch, at which the ascending node crosses the equator. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.");
        add_attribute(datasets, "long_name", "Ascending Node Crossing Time");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "seconds since 2018-01-01");
        add_attribute(datasets, "standard_name", "time");
        goto_parent(datasets);

        /* Create Variable - cycle_number */
        add_scalar(datasets, "cycle_number", last_df->getMetaData("cycle"));
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "Tracks the number of 91-day cycles in the mission, beginning with 01.  A unique orbit number can be determined by subtracting 1 from the cycle_number, multiplying by 1387 and adding the rgt value.");
        add_attribute(datasets, "long_name", "Cycle Number");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "counts");
        goto_parent(datasets);

        /* Create Variable - lan */
        add_scalar(datasets, "lan", &granule["lan"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "Longitude at the ascending node crossing.");
        add_attribute(datasets, "long_name", "Ascending Node Longitude");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "degrees_east");
        goto_parent(datasets);

        /* Create Variable - orbit_number */
        add_scalar(datasets, "orbit_number", &granule["orbit_number"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "Unique identifying number for each planned ICESat-2 orbit.");
        add_attribute(datasets, "long_name", "Orbit Number");
        add_attribute(datasets, "source", "Operations");
        add_attribute(datasets, "units", "1");
        goto_parent(datasets);

        /* Create Variable - rgt */
        add_scalar(datasets, "rgt", last_df->getMetaData("rgt"));
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "The reference ground track (RGT) is the track on the earth at which a specified unit vector within the observatory is pointed. Under nominal operating conditions, there will be no data collected along the RGT, as the RGT is spanned by GT2L and GT2R.  During slews or off-pointing, it is possible that ground tracks may intersect the RGT. The ICESat-2 mission has 1387 RGTs.");
        add_attribute(datasets, "long_name", "Reference Ground Track");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "counts");
        goto_parent(datasets);

        /* Create Variable - sc_orient */
        add_scalar(datasets, "sc_orient", &granule["sc_orient"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "This parameter tracks the spacecraft orientation between forward, backward and transitional flight modes. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode.");
        add_attribute(datasets, "long_name", "Spacecraft Orientation");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "1");
        add_attribute(datasets, "flag_meanings", "backward forward transition");
        add_attribute(datasets, "flag_values", "0, 1, 2");
        goto_parent(datasets);

        /* Create Variable - sc_orient_time */
        add_scalar(datasets, "sc_orient_time", &granule["sc_orient_time"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "The time of the last spacecraft orientation change between forward, backward and transitional flight modes, expressed in seconds since the ATLAS SDP GPS Epoch. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.");
        add_attribute(datasets, "long_name", "Time of Last Spacecraft Orientation Change");
        add_attribute(datasets, "source", "POD/PPD");
        add_attribute(datasets, "units", "seconds since 2018-01-01");
        add_attribute(datasets, "standard_name", "time");
        goto_parent(datasets);

        /* Create Variable - bounding_polygon_lat1 */
        add_variable(datasets, "bounding_polygon_lat1", &granule["lat_poly"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "Latitude values for the first out of two possible bounding polygons");
        add_attribute(datasets, "long_name", "Polygon1 Latitude");
        add_attribute(datasets, "source", "geo_poly");
        add_attribute(datasets, "units", "degrees_north");
        add_attribute_double(datasets, "valid_max", 90.0);
        add_attribute_double(datasets, "valid_min", -90.0);
        goto_parent(datasets);

        /* Create Variable - bounding_polygon_lon1 */
        add_variable(datasets, "bounding_polygon_lon1", &granule["lon_poly"]);
        add_attribute(datasets, "contentType", "referenceInformation");
        add_attribute(datasets, "description", "Longitude values for the first out of two possible bounding polygons");
        add_attribute(datasets, "long_name", "Polygon1 Longitude");
        add_attribute(datasets, "source", "geo_poly");
        add_attribute(datasets, "units", "degrees_east");
        add_attribute_double(datasets, "valid_max", 180.0);
        add_attribute_double(datasets, "valid_min", -180.0);
        goto_parent(datasets);

        /* Go Back to Parent Group */
        goto_parent(datasets);

        /*************************/
        /* Create Metadata Group */
        /*************************/
        add_group(datasets, "metadata");

        /* Create Variable - sliderule */
        add_scalar(datasets, "sliderule", &rqst_metadata);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "sliderule server and request information");
        add_attribute(datasets, "long_name", "SlideRule MetaData");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "json");
        goto_parent(datasets);

        /* Create Variable - atl24 */
        add_scalar(datasets, "atl24", &atl24_metadata);
        add_attribute(datasets, "contentType", "auxiliaryInformation");
        add_attribute(datasets, "description", "atl24 algorithm versioning and build information");
        add_attribute(datasets, "long_name", "ATL24 MetaData");
        add_attribute(datasets, "source", "Derived");
        add_attribute(datasets, "units", "json");
        goto_parent(datasets);

        /* Create Group - DatasetIdentification */
        TimeLib::gmt_time_t gmt = TimeLib::gmttime();
        TimeLib::date_t date = TimeLib::gmt2date(gmt);
        FString creationDate("%d-%02d-%02dT%02d:%02d:%02d.000000Z", date.year, date.month, date.day, gmt.hour, gmt.minute, gmt.second);
        char uuid_str[UUID_STR_LEN];
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        const char* datasetFileName = StringLib::find(parms->output.path.value.c_str(), '/', false);
        if(datasetFileName) datasetFileName += 1;
        else datasetFileName = parms->output.path.value.c_str();
        add_group(datasets, "DatasetIdentification");
        add_attribute(datasets, "spatialRepresentationType", "along-track");
        add_attribute(datasets, "creationDate", creationDate.c_str());
        add_attribute(datasets, "uuid", uuid_str);
        add_attribute(datasets, "fileName", datasetFileName);
        add_attribute(datasets, "VersionID", FString("0%s", lua_obj->release.value.c_str()).c_str());
        add_attribute(datasets, "language", "eng");
        add_attribute(datasets, "characterSet", "utf8");
        add_attribute(datasets, "shortName", "ATL24");
        add_attribute(datasets, "originatorOrganizationName", "SlideRule");
        add_attribute(datasets, "abstract", "ATL24 provides global along-track coastal and nearshore bathymetry, consisting of refraction corrected seafloor and sea surface heights (orthometric and ellipsoidal heights and instantaneous depths), as well as associated uncertainties.");
        add_attribute(datasets, "purpose", "The purpose of ATL24 is to provide photon level classification of sea surface and seafloor for ATL03 data in coastal and nearshore regions.");
        add_attribute(datasets, "credit", "The ATL24 product was designed and implemented by a team led by Lori Magruder at the University of Texas at Austin, and Christopher Parrish at Oregon State University.");
        add_attribute(datasets, "status", "onGoing");
        add_attribute(datasets, "topicCategory", "geoscientificInformation");
        goto_parent(datasets);

        /* Go Back to Parent Group */
        goto_parent(datasets);

        /*******************/
        /* Write HDF5 File */
        /*******************/
        mlog(INFO, "Writing HDF5 file: %s", filename);
        status = HdfLib::write(filename, datasets);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error writing file: %s", e.what());
        status = false;
    }

    /* Clean Up */
    for(int i = 0; i < datasets.length(); i++)
    {
        delete [] datasets[i].data;
    }

    /* Return */
    return returnLuaStatus(L, status);
}
