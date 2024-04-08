local runner = require("test_executive")
console = require("console")
asset = require("asset")
local assets = asset.loaddir()
local td = runner.rootdir(arg[0])

local in_file = td.."atl06_10rows.geoparquet"
local out_file = td.."samples.geoparquet"

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

function getFileSize(filePath)
    local file = io.open(filePath, "rb")  -- 'rb' mode opens the file in binary mode
    if not file then
        print("Could not open file: " .. filePath)
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

local parquet_sampler = arrow.parquetsampler(arrow.parms({path=out_file, format="parquet", as_geo=true}), in_file, {["mosaic"] = dem1, ["strips"] = dem2})
runner.check(parquet_sampler ~= nil)

print('\n------------------\nTest01: parquetsampler sample \n------------------')

local in_file_size = getFileSize(in_file);
print("Input  file size: " .. in_file_size .. " bytes")

local status = parquet_sampler:sample()
local out_file_size = getFileSize(out_file);
print("Output file size: " .. out_file_size .. " bytes")
runner.check(out_file_size > in_file_size, "Outpu file size is not greater than input file size: ")

print('\n------------------\nTest02: parquetsampler sample again \n------------------')

status = parquet_sampler:sample()
new_out_file_size = getFileSize(out_file)
print("Output file size: " .. out_file_size .. " bytes")
runner.check(out_file_size == new_out_file_size, "Output file size has incorrectly changed: ")


-- There is no easy way to read parquet file in Lua, check the size of the output file
-- the file was tested with python and it has the expected content

-- Remove the output file
os.remove(out_file)

-- Report Results --

runner.report()

