local runner = require("test_executive")
console = require("console")
asset = require("asset")
local assets = asset.loaddir()
local td = runner.rootdir(arg[0])

local outq_name = "outq-luatest"

local in_geoparquet  = td.."atl06_10rows.geoparquet"
local in_parquet     = td.."atl06_10rows.parquet"

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

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

function getFileSize(filePath)
    local file = io.open(filePath, "rb")  -- 'rb' mode opens the file in binary mode
    if not file then
        print("Could not open file: " .. filePath .. " for reading\n")
        return nil
    end
    local fileSize = file:seek("end")  -- Go to the end of the file
    file:close()  -- Close the file
    return fileSize
end

local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true}))
runner.check(dem1 ~= nil)

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, with_flags=true, use_poi_time=true}))
runner.check(dem2 ~= nil)

print('\n--------------------------------------\nTest01: input/output geoparquet (geo)\n--------------------------------------')
local parquet_sampler = arrow.sampler(arrow.parms({path=out_geoparquet, format="parquet"}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.check(parquet_sampler ~= nil)

local in_file_size = getFileSize(in_geoparquet);
print("Input  geoparquet file size: " .. in_file_size .. " bytes")

local status = parquet_sampler:sample()
local out_file_size = getFileSize(_out_geoparquet);
print("Output geoparquet file size: " .. out_file_size .. " bytes")
runner.check(out_file_size > in_file_size, "Output file size is not greater than input file size: ")


print('\n--------------------------------------\nTest02: input/output parquet (x, y)\n--------------------------------------')
parquet_sampler = arrow.sampler(arrow.parms({path=out_parquet, format="parquet"}), in_parquet, outq_name, {["mosaic"] = dem1})
runner.check(parquet_sampler ~= nil)

in_file_size = getFileSize(in_parquet);
print("Input  parquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:sample()
out_file_size = getFileSize(_out_parquet);
print("Output parquet file size: " .. out_file_size .. " bytes")
runner.check(out_file_size > in_file_size, "Output file size is not greater than input file size: ")

--NOTE: generated CSV and FEATHER files are much smaller than the input parquet/geoparquet files

print('\n--------------------------------------\nTest03: input geoparquet, output feather\n--------------------------------------')
parquet_sampler = arrow.sampler(arrow.parms({path=out_feather, format="feather"}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.check(parquet_sampler ~= nil)

in_file_size = getFileSize(in_geoparquet);
print("Input geoparquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:sample()
out_file_size = getFileSize(_out_feather);
print("Output parquet file size:    " .. out_file_size .. " bytes")
runner.check(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.check(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest04: input parquet, output feather\n--------------------------------------')
parquet_sampler = arrow.sampler(arrow.parms({path=out_feather, format="feather"}), in_parquet, outq_name, {["mosaic"] = dem1})
runner.check(parquet_sampler ~= nil)

in_file_size = getFileSize(in_parquet);
print("Input geoparquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:sample()
out_file_size = getFileSize(_out_feather);
print("Output parquet file size:    " .. out_file_size .. " bytes")
runner.check(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.check(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest05: input geoparquet, output CSV\n--------------------------------------')
parquet_sampler = arrow.sampler(arrow.parms({path=out_csv, format="csv"}), in_geoparquet, outq_name, {["mosaic"] = dem1})
runner.check(parquet_sampler ~= nil)

in_file_size = getFileSize(in_geoparquet);
print("Input geoparquet file size: " .. in_file_size .. " bytes")

status = parquet_sampler:sample()
out_file_size = getFileSize(_out_csv);
print("Output CSV file size:        " .. out_file_size .. " bytes")
runner.check(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size:   " .. meta_file_size .. " bytes")
runner.check(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest06: input parquet, output CSV \n--------------------------------------')
parquet_sampler = arrow.sampler(arrow.parms({path=out_csv, format="csv"}), in_parquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.check(parquet_sampler ~= nil)

in_file_size = getFileSize(in_parquet);
print("Input parquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:sample()
out_file_size = getFileSize(_out_csv);
print("Output CSV file size:     " .. out_file_size .. " bytes")
runner.check(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
meta_file_size = getFileSize(_out_metadata);
print("Output metadata file size: " .. meta_file_size .. " bytes")
runner.check(meta_file_size > 0, "Output metadata json file size is empty: ")
os.remove(_out_metadata)


print('\n--------------------------------------\nTest07: input/output geoparquet (geo)\n--------------------------------------')
local parquet_sampler = arrow.sampler(arrow.parms({path=out_geoparquet, format="parquet"}), in_geoparquet, outq_name, {["mosaic"] = dem1, ["strips"] = dem2})
runner.check(parquet_sampler ~= nil)

local in_file_size = getFileSize(in_geoparquet);
print("Input  geoparquet file size: " .. in_file_size .. " bytes")

local status = parquet_sampler:sample()
local out_file_size = getFileSize(_out_geoparquet);
print("Output geoparquet file size: " .. out_file_size .. " bytes")
runner.check(out_file_size > in_file_size, "Output file size is not greater than input file size: ")


-- There is no easy way to read parquet file in Lua, check the size of the output files
-- the files were tested with python scripts

-- Remove the output files
os.remove(_out_geoparquet)
os.remove(_out_parquet)
os.remove(_out_feather)
os.remove(_out_csv)
os.remove(_out_metadata)

-- Report Results --

runner.report()

