local runner = require("test_executive")
console = require("console")
asset = require("asset")
local assets = asset.loaddir()
local td = runner.rootdir(arg[0])

local in_file = td.."atl06_5rows.parquet"
local out_file = td.."samples.parquet"

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

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, with_flags=true}))
runner.check(dem2 ~= nil)

local parquet_sampler = arrow.parquetsampler(in_file, out_file, {dem1, dem2})
runner.check(parquet_sampler ~= nil)

print('\n------------------\nTest01: parquetsampler sample \n------------------')

-- For performance limit strips rasters to one strip by using tstr filter
local tstr = "2023:2:4:23:3:0"
local status = parquet_sampler:sample(tstr)

print('\n------------------\nTest02: parquet file creation \n------------------')

-- There is no easy way to read parquet file in Lua, check the size of the output file
-- the file was tested with python and it has the expected content
local size = getFileSize(out_file);
local expected_size = 18606
runner.check(size == expected_size, "Expected output file size: " .. expected_size .. " bytes, but got: " .. size .. " bytes")

-- For multiple strips for each point use this instead
-- local status = parquet_sampler:sample()
-- local size = getFileSize(out_file);
-- local expected_size = 29749
-- runner.check(size == expected_size, "Expected output file size: " .. expected_size .. " bytes, but got: " .. size .. " bytes")

-- Remove the output file
os.remove(out_file)

-- Report Results --

runner.report()

