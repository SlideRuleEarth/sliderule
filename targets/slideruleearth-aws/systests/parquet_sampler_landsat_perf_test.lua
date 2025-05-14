local runner = require("test_executive")

-- Setup --

runner.authenticate()

local outq_name = "outq-luatest"

-- Use the same input parquet files as 3dep
-- local in_parquet = '/data/3dep/wrzesien_snow_64k.parquet'
-- local in_parquet = '/data/3dep/wrzesien_snow_525k.parquet'
local in_parquet = '/data/3dep/wrzesien_snow_2618k.parquet'

-- Indicates local file system (no s3 or client)
local prefix = "file://"

local _out_parquet   = "/data/landsat/output/wrzesien_snow_samples.parquet"
local out_parquet    = prefix .. _out_parquet

local geojsonfile = "/data/landsat/wrzesien_mountain_snow_sieve_catalog.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()


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

local robj_hls = geo.raster(geo.parms({ asset = "landsat-hls", algorithm = "NearestNeighbour", radius = 0, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, use_poi_time=false}))
runner.assert(robj_hls ~= nil)

print('\n--------------------------------------\nTest01: input/output parquet (x, y)\n--------------------------------------')
local starttime = time.latch();
parquet_sampler = arrow.sampler(core.parms({output={path=out_parquet, format="parquet"}}), in_parquet, outq_name, {["hls"] = robj_hls})
runner.assert(parquet_sampler ~= nil)
status = parquet_sampler:waiton()
local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("ExecTime: %f", dtime))


in_file_size = getFileSize(in_parquet);
print("Input  parquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_parquet);
print("Output parquet file size: " .. out_file_size .. " bytes")
runner.assert(out_file_size > in_file_size, "Output file size is not greater than input file size: ")

-- There is no easy way to read parquet file in Lua, check the size of the output files
-- the files were tested with python scripts

-- Remove the output files
os.remove(_out_parquet)

-- Report Results --

runner.report()

