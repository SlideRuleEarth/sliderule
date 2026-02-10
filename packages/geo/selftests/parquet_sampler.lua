local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Setup --

local outq_name = "outq-luatest"

local in_geoparquet  = dirpath.."../data/atl06_10rows.geoparquet"
local in_parquet     = dirpath.."../data/atl06_10rows.parquet"

local geojsonfile = dirpath.."../data/arcticdem_strips.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file")
if not f then return end
local contents = f:read("*all")
f:close()

-- Indicates local file system (no s3 or client)
local prefix = "file://"

local function getFileSize(filePath)
    local file = io.open(filePath, "rb")  -- 'rb' mode opens the file in binary mode
    if not file then
        print("Could not open file: " .. filePath .. " for reading\n")
        return nil
    end
    local fileSize = file:seek("end")  -- Go to the end of the file
    file:close()  -- Close the file
    return fileSize
end

local function findMatchingString(filePath, searchTerm)
    local file = assert(io.open(filePath, "rb"))
    local data = file:read("*all")
    file:close()
    return string.find(data, searchTerm, 1, true)
end

local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true, with_flags=true, slope_aspect=true, slope_scale_length=40 }))
runner.assert(dem1 ~= nil)

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, slope_aspect=true, slope_scale_length=0, use_poi_time=true}))
runner.assert(dem2 ~= nil)

-- Self Test --

runner.unittest("Input/Output GeoParquet (single)", function()
    local _out_geoparquet = "/tmp/samples.geoparquet"
    local out_geoparquet = prefix .. _out_geoparquet
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
    runner.assert(parquet_sampler ~= nil)
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_geoparquet);
    runner.assert(out_file_size > 0, "Output file is empty")
    runner.assert(findMatchingString(_out_geoparquet, "mosaic"), "Could not find key in output file")
    os.remove(_out_geoparquet)
end)

runner.unittest("Input/Output GeoParquet (x, y)", function()
    local _out_parquet = "/tmp/samples.parquet"
    local out_parquet = prefix .. _out_parquet
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_parquet, format="parquet"}}), in_parquet, outq_name, {["mosaic"] = dem1})
    runner.assert(parquet_sampler ~= nil)
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_parquet);
    runner.assert(out_file_size > 0, "Output file is empty")
    runner.assert(findMatchingString(_out_parquet, "mosaic"), "Could not find key in output file")
    os.remove(_out_parquet)
end)

runner.unittest("Input GeoParquet, Output Feather", function()
    local _out_feather = "/tmp/samples.feather"
    local out_feather = prefix .. _out_feather
    local _out_metadata = "/tmp/samples_metadata.json"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_feather, format="feather"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
    runner.assert(parquet_sampler ~= nil)
    local in_file_size = getFileSize(in_geoparquet);
    print("Input geoparquet file size: "  .. in_file_size .. " bytes")
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_feather);
    print("Output parquet file size:    " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
    local meta_file_size = getFileSize(_out_metadata);
    print("Output metadata file size:   " .. meta_file_size .. " bytes")
    runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
    os.remove(_out_metadata)
    os.remove(_out_feather)
end)

runner.unittest("Input Parquet, Output Feather", function()
    local _out_feather = "/tmp/samples.feather"
    local out_feather = prefix .. _out_feather
    local _out_metadata = "/tmp/samples_metadata.json"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_feather, format="feather"}}), in_parquet, outq_name, {["mosaic"] = dem1})
    runner.assert(parquet_sampler ~= nil)
    local in_file_size = getFileSize(in_parquet);
    print("Input geoparquet file size: "  .. in_file_size .. " bytes")
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_feather);
    print("Output parquet file size:    " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
    local meta_file_size = getFileSize(_out_metadata);
    print("Output metadata file size:   " .. meta_file_size .. " bytes")
    runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
    os.remove(_out_metadata)
    os.remove(_out_feather)
end)

runner.unittest("Input GeoParquet, Output CSV", function()
    local _out_csv = "/tmp/samples.csv"
    local out_csv = prefix .. _out_csv
    local _out_metadata = "/tmp/samples_metadata.json"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_csv, format="csv"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
    runner.assert(parquet_sampler ~= nil)
    local in_file_size = getFileSize(in_geoparquet);
    print("Input geoparquet file size: " .. in_file_size .. " bytes")
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_csv);
    print("Output CSV file size:        " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
    local meta_file_size = getFileSize(_out_metadata);
    print("Output metadata file size:   " .. meta_file_size .. " bytes")
    runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
    os.remove(_out_metadata)
    os.remove(_out_csv)
end)

runner.unittest("Input Parquet, Output CSV", function()
    local _out_csv = "/tmp/samples.csv"
    local out_csv = prefix .. _out_csv
    local _out_metadata = "/tmp/samples_metadata.json"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_csv, format="csv"}}), in_parquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler ~= nil)
    local in_file_size = getFileSize(in_parquet);
    print("Input parquet file size: "  .. in_file_size .. " bytes")
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_csv);
    print("Output CSV file size:     " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
    local meta_file_size = getFileSize(_out_metadata);
    print("Output metadata file size: " .. meta_file_size .. " bytes")
    runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
    os.remove(_out_metadata)
    os.remove(_out_csv)
end)

runner.unittest("Input/Output GeoParquet (multiple)", function()
    local _out_geoparquet = "/tmp/samples.geoparquet"
    local out_geoparquet = prefix .. _out_geoparquet
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler ~= nil)
    parquet_sampler:waiton()
    local out_file_size = getFileSize(_out_geoparquet);
    runner.assert(out_file_size > in_file_size, "Output file is empty")
    runner.assert(findMatchingString(_out_geoparquet, "mosaic"), "Could not find key in output file")
    runner.assert(findMatchingString(_out_geoparquet, "strips"), "Could not find key in output file")
    os.remove(_out_geoparquet)
end)

runner.unittest("Bad Arrow Parameters (failed luaCreate)", function()
    local parquet_sampler = arrow.sampler(core.parms(), in_geoparquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler == nil)
end)

runner.unittest("Empty Input File (failed constructor)", function()
    local out_geoparquet = prefix .. "/tmp/samples.geoparquet"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), "", outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler == nil)
end)

runner.unittest("Nil Input File (failed constructor)", function()
    local out_geoparquet = prefix .. "/tmp/samples.geoparquet"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), nil, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler == nil)
end)

runner.unittest("Nil Output Queue (failed constructor)", function()
    local out_geoparquet = prefix .. "/tmp/samples.geoparquet"
    local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, nil, {["mosaic"] = dem1, ["strips"] = dem2})
    runner.assert(parquet_sampler == nil)
end)

-- Report Results --

runner.report()
