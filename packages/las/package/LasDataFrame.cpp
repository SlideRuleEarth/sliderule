/*
 * Copyright (c) 2024, University of Washington
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
#include "LasDataFrame.h"
#include "OutputLib.h"

#include <pdal/PointTable.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/BufferReader.hpp>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LasDataFrame::OBJECT_TYPE = "LasDataFrame";
const char* LasDataFrame::LUA_META_NAME = "LasDataFrame";
const struct luaL_Reg LasDataFrame::LUA_META_TABLE[] = {
    {"export",  luaExport},
    {NULL,      NULL}
};


/******************************************************************************
* computeLasScale() template helper function
*
* LAS files do not store coordinate values (X, Y, Z) as floating-point numbers.
* Instead, each coordinate is stored as a 32-bit signed integer that is scaled
* and offset according to the LAS header values:
*
*     ActualValue = (StoredInt32 * scale) + offset
*
* The "scale" determines numeric precision (smaller scales = higher precision),
* while the "offset" anchors the coordinate range so that all points fit safely
* within the ±2,147,483,647 limit of a signed 32-bit integer.
*
* This function computes the smallest possible LAS scale factor for a given
* data column such that all finite values fit in the 32-bit integer range,
* maximizing precision without overflow.  The offset is set to the minimum
* finite value in the column so that stored integers are non-negative and
* centered near zero.
*
* For example, a longitude column spanning only 0.01° at a scale of 1e-9
* results in sub-centimeter precision and comfortably fits in the LAS range.
*
* Note: GPS time in LAS is stored as a 64-bit floating-point (double) value
* in seconds, so it is written directly without scale/offset quantization.
* Only spatial dimensions (X, Y, Z) are integer-scaled.
*
* This approach preserves the full available precision of the input doubles
* (and floats) while ensuring the output conforms to the LAS specification
* and can be read by all compliant tools.
******************************************************************************/

namespace
{
    template<typename T>
    double computeLasScale(const FieldColumn<T>* column, double fallback_scale, double& offset)
    {
        offset = 0.0;
        if(column == NULL) return fallback_scale;

        const long length = column->length();
        bool have_finite_value = false;
        double min_value = 0.0;
        double max_value = 0.0;

        for(long i = 0; i < length; ++i)
        {
            const double value = static_cast<double>((*column)[i]);
            if(!std::isfinite(value)) continue;

            if(!have_finite_value)
            {
                min_value = value;
                max_value = value;
                have_finite_value = true;
            }
            else
            {
                min_value = std::min(min_value, value);
                max_value = std::max(max_value, value);
            }
        }

        if(!have_finite_value)
        {
            offset = 0.0;
            return fallback_scale;
        }

        offset = min_value;
        const double range = max_value - min_value;
        if(range <= 0.0 || !std::isfinite(range))
            return fallback_scale;

        // Ensure range fits within 32-bit LAS integer range
        constexpr double max_int = static_cast<double>(std::numeric_limits<int32_t>::max()) - 1.0;
        double scale = range / max_int;

        // Clamp to reasonable lower bound
        if(scale < 1e-12 || !std::isfinite(scale))
            scale = fallback_scale;

        // Slightly nudge upward to avoid rounding to zero
        scale = std::nextafter(scale, std::numeric_limits<double>::infinity());
        return scale;
    }
}

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int LasDataFrame::luaCreate (lua_State* L)
{
    RequestFields* parms = NULL;
    GeoDataFrame* dataframe = NULL;

    try
    {
        parms = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        dataframe = dynamic_cast<GeoDataFrame*>(getLuaObject(L, 2, GeoDataFrame::OBJECT_TYPE));
        return createLuaObject(L, new LasDataFrame(L, parms, dataframe));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        if(dataframe) dataframe->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport
 *----------------------------------------------------------------------------*/
int LasDataFrame::luaExport (lua_State* L)
{
    try
    {
        LasDataFrame* lua_obj = dynamic_cast<LasDataFrame*>(getLuaSelf(L, 1));
        if(lua_obj == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid LasDataFrame object");

        const GeoDataFrame* dataframe = lua_obj->dataframe;
        const RequestFields* parms = lua_obj->parms;
        const OutputFields::format_t default_format = parms->output.format.value;

        const FieldColumn<double>* x_column = dataframe->getXColumn();
        const FieldColumn<double>* y_column = dataframe->getYColumn();
        const FieldColumn<float>*  z_column = dataframe->getZColumn();
        const FieldColumn<time8_t>* time_column = dataframe->getTimeColumn();

        if(x_column == NULL || y_column == NULL || z_column == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "dataframe missing required geometry columns (x/y/z)");
        }

        if(time_column == NULL)
        {
            mlog(WARNING, "dataframe missing time column; proceeding without per-point timestamps");
        }

        const long num_points = dataframe->length();
        if(num_points <= 0)
        {
            throw RunTimeException(INFO, RTE_FAILURE, "dataframe has no rows to export");
        }

        const char* tmp_name = OutputLib::getUniqueFileName("las");
        std::string default_filename = tmp_name ? tmp_name : "/tmp/las.bin";
        delete [] tmp_name;

        const char* default_extension = (default_format == OutputFields::LAZ) ? ".laz" : ".las";
        const size_t dot_pos = default_filename.find_last_of('.');
        if(dot_pos != std::string::npos) default_filename.resize(dot_pos);
        default_filename.append(default_extension);

        const char* filename_arg = getLuaString(L, 2, true, default_filename.c_str());
        const OutputFields::format_t format = static_cast<OutputFields::format_t>(getLuaInteger(L, 3, true, default_format));

        if(format != OutputFields::LAS && format != OutputFields::LAZ)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid LAS export format: %d", static_cast<int>(format));
        }

        std::string output_filename = filename_arg ? filename_arg : "";
        if(output_filename.empty())
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid filename specified for LAS export");
        }

        const std::string desired_ext = (format == OutputFields::LAZ) ? ".laz" : ".las";
        if(output_filename.size() < desired_ext.size() ||
           output_filename.rfind(desired_ext) != (output_filename.size() - desired_ext.size()))
        {
            const size_t user_dot = output_filename.find_last_of('.');
            if(user_dot != std::string::npos)
            {
                output_filename.resize(user_dot);
            }
            output_filename.append(desired_ext);
        }

        pdal::PointTable table;
        pdal::PointLayoutPtr layout = table.layout();
        layout->registerDim(pdal::Dimension::Id::X);
        layout->registerDim(pdal::Dimension::Id::Y);
        layout->registerDim(pdal::Dimension::Id::Z);
        layout->registerDim(pdal::Dimension::Id::Intensity);
        layout->registerDim(pdal::Dimension::Id::NumberOfReturns);
        layout->registerDim(pdal::Dimension::Id::ReturnNumber);
        layout->registerDim(pdal::Dimension::Id::PointSourceId);
        if(time_column) layout->registerDim(pdal::Dimension::Id::GpsTime);
        table.finalize();

        pdal::SpatialReference srs;
        const std::string& crs = dataframe->getCRS();
        if(!crs.empty())
        {
            srs = pdal::SpatialReference(crs);
            table.setSpatialReference(srs);
        }

        const pdal::PointViewPtr view = pdal::PointViewPtr(new pdal::PointView(table, srs));

        for(long i = 0; i < num_points; i++)
        {
            const pdal::PointId idx = view->size();
            view->setField(pdal::Dimension::Id::X, idx, (*x_column)[i]);
            view->setField(pdal::Dimension::Id::Y, idx, (*y_column)[i]);
            view->setField(pdal::Dimension::Id::Z, idx, static_cast<double>((*z_column)[i]));
            view->setField(pdal::Dimension::Id::Intensity, idx, static_cast<uint16_t>(1));
            view->setField(pdal::Dimension::Id::NumberOfReturns, idx, static_cast<uint8_t>(1));
            view->setField(pdal::Dimension::Id::ReturnNumber, idx, static_cast<uint8_t>(1));
            view->setField(pdal::Dimension::Id::PointSourceId, idx, static_cast<uint16_t>(0));

            if(time_column)
            {
                const time8_t t = (*time_column)[i];

                // Convert UNIX nanoseconds to GPS seconds (LAS-compliant)
                const int64_t gps_millisecs = TimeLib::sys2gpstime(t.nanoseconds / 1000);
                const double gps_seconds = static_cast<double>(gps_millisecs) / 1000.0;
                view->setField(pdal::Dimension::Id::GpsTime, idx, gps_seconds);
            }
        }

        const double fallback_double_scale = static_cast<double>(std::numeric_limits<double>::epsilon());
        const double fallback_float_scale = static_cast<double>(std::numeric_limits<float>::epsilon());

        double offset_x = 0.0;
        double offset_y = 0.0;
        double offset_z = 0.0;

        const double scale_x = computeLasScale(x_column, fallback_double_scale, offset_x);
        const double scale_y = computeLasScale(y_column, fallback_double_scale, offset_y);
        const double scale_z = computeLasScale(z_column, fallback_float_scale, offset_z);

        pdal::BufferReader reader;
        reader.addView(view);

        pdal::Options writer_options;
        writer_options.add("filename", output_filename);
        if(format == OutputFields::LAZ)
        {
            writer_options.add("compression", "laszip");
        }

        // Force modern LAS 1.4 format that supports CRS/WKT
        writer_options.add("minor_version", 4);
        writer_options.add("dataformat_id", 6);

        writer_options.add("forward", "all");

        // Set computed scale/offset values for coordinate fields
        writer_options.add("scale_x", scale_x);
        writer_options.add("scale_y", scale_y);
        writer_options.add("scale_z", scale_z);
        writer_options.add("offset_x", offset_x);
        writer_options.add("offset_y", offset_y);
        writer_options.add("offset_z", offset_z);

        pdal::StageFactory factory;
        pdal::Stage* writer_stage = factory.createStage("writers.las");
        if(writer_stage == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to create PDAL writers.las stage");
        }

        writer_stage->setInput(reader);
        writer_stage->setOptions(writer_options);
        writer_stage->prepare(table);
        writer_stage->execute(table);

        lua_pushstring(L, output_filename.c_str());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Point cloud export failed: %s", e.what());
        lua_pushnil(L);
    }
    catch(const std::exception& e)
    {
        mlog(CRITICAL, "Error exporting LAS/LAZ: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/******************************************************************************
 * CONSTRUCTORS / DESTRUCTORS
 ******************************************************************************/

LasDataFrame::LasDataFrame(lua_State* L, RequestFields* _parms, GeoDataFrame* _dataframe):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    dataframe(_dataframe)
{
    assert(parms);
    assert(dataframe);
}

LasDataFrame::~LasDataFrame()
{
    parms->releaseLuaObject();
    dataframe->releaseLuaObject();
}
