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
#include <pdal/PointView.hpp>
#include <pdal/PointLayout.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Options.hpp>
#include <pdal/SpatialReference.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/BufferReader.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

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

        const std::string& crs = dataframe->getCRS();
        pdal::PointViewPtr view;
        if(!crs.empty())
        {
            const pdal::SpatialReference srs(crs);
            table.setSpatialReference(srs);
            view = pdal::PointViewPtr(new pdal::PointView(table, srs));
        }
        else
        {
            view = pdal::PointViewPtr(new pdal::PointView(table));
        }

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
                const double gps_seconds = static_cast<double>(t.nanoseconds) * 1e-9;
                view->setField(pdal::Dimension::Id::GpsTime, idx, gps_seconds);
            }
        }

        pdal::BufferReader reader;
        reader.addView(view);

        pdal::Options writer_options;
        writer_options.add("filename", output_filename);
        if(format == OutputFields::LAZ)
        {
            writer_options.add("compression", "laszip");
        }

        pdal::StageFactory factory;
        pdal::Stage* writer_stage = factory.createStage("writers.las");
        if(writer_stage == nullptr)
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
