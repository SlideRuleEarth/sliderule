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

local _out_geoparquet = "/tmp/samples.geoparquet"
local _out_parquet    = "/tmp/samples.parquet"
local _out_feather    = "/tmp/samples.feather"
local _out_csv        = "/tmp/samples.csv"
local _out_metadata   = "/tmp/samples_metadata.json"

local out_geoparquet = prefix .. _out_geoparquet
local out_parquet    = prefix .. _out_parquet
local out_feather    = prefix .. _out_feather
local out_csv        = prefix .. _out_csv
local out_metadata   = prefix .. _out_metadata

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

-- Self Test --

local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true}))
runner.assert(dem1 ~= nil)

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, use_poi_time=true}))
runner.assert(dem2 ~= nil)

print('\n--------------------------------------\nTest01: input/output geoparquet (geo)\n--------------------------------------')
local parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.assert(parquet_sampler ~= nil)
local status = parquet_sampler:waiton()
local out_file_size = getFileSize(_out_geoparquet);
runner.assert(out_file_size > 0, "Output file is empty")
runner.assert(findMatchingString(_out_geoparquet, "mosaic"), "Could not find key in output file")

print('\n--------------------------------------\nTest02: input/output parquet (x, y)\n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_parquet, format="parquet"}}), in_parquet, outq_name, {["mosaic"] = dem1})
runner.assert(parquet_sampler ~= nil)
status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_parquet);
runner.assert(out_file_size > 0, "Output file is empty")
runner.assert(findMatchingString(_out_parquet, "mosaic"), "Could not find key in output file")

--NOTE: generated CSV and FEATHER files are much smaller than the input parquet/geoparquet files

print('\n--------------------------------------\nTest03: input geoparquet, output feather\n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_feather, format="feather"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.assert(parquet_sampler ~= nil)

in_file_size = getFileSize(in_geoparquet);
print("Input geoparquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_feather);
print("Output parquet file size:    " .. out_file_size .. " bytes")
runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest04: input parquet, output feather\n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_feather, format="feather"}}), in_parquet, outq_name, {["mosaic"] = dem1})
runner.assert(parquet_sampler ~= nil)

in_file_size = getFileSize(in_parquet);
print("Input geoparquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_feather);
print("Output parquet file size:    " .. out_file_size .. " bytes")
runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest05: input geoparquet, output CSV\n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_csv, format="csv"}}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.assert(parquet_sampler ~= nil)

in_file_size = getFileSize(in_geoparquet);
print("Input geoparquet file size: " .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_csv);
print("Output CSV file size:        " .. out_file_size .. " bytes")
runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest06: input parquet, output CSV \n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_csv, format="csv"}}), in_parquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler ~= nil)

in_file_size = getFileSize(in_parquet);
print("Input parquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_csv);
print("Output CSV file size:     " .. out_file_size .. " bytes")
runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size: " .. meta_file_size .. " bytes")
runner.assert(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest07: input/output geoparquet (geo)\n--------------------------------------')
parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler ~= nil)
status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_geoparquet);
runner.assert(out_file_size > in_file_size, "Output file is empty")
runner.assert(findMatchingString(_out_geoparquet, "mosaic"), "Could not find key in output file")
runner.assert(findMatchingString(_out_geoparquet, "strips"), "Could not find key in output file")

-- Negative tests

print('\n--------------------------------------\nTest08: failed luaCreate bad arrow parms\n--------------------------------------\n')
parquet_sampler = arrow.sampler(core.parms(), in_geoparquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler == nil)

print('\n--------------------------------------\nTest09: failed constructor empty input file\n--------------------------------------\n')
parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), "", outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler == nil)

print('\n--------------------------------------\nTest10: failed constructor nil input file\n--------------------------------------\n')
parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), nil, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler == nil)

print('\n--------------------------------------\nTest10: failed constructor nil output queue\n--------------------------------------\n')
parquet_sampler = arrow.sampler(core.parms({output={path=out_geoparquet, format="parquet"}}), in_geoparquet, nil, {["mosaic"] = dem1, ["strips"] = dem2})
runner.assert(parquet_sampler == nil)

-- There is no easy way to read parquet file in Lua, check the size of the output files
-- the files were tested with python scripts

-- Clean Up --

--os.remove(_out_geoparquet)
os.remove(_out_parquet)
os.remove(_out_feather)
os.remove(_out_csv)
os.remove(_out_metadata)

-- Report Results --

runner.report()
