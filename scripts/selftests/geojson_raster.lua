local runner = require("test_executive")
local json = require("json")
local console = require("console")
local td = runner.rootdir(arg[0])

-- Setup --

local geojsonfile = td.."/grandmesa.geojson"

-- Unit Test --

local f = io.open(geojsonfile, "r")
local vectorfile = nil

if f ~= nil then
    vectorfile = f:read("*a")
    f:close()
else
    runner.check(false, "failed to open geojson file")
end

local len = string.len(vectorfile)
local cellsize = 0.1
local params = {data = vectorfile, length = len, cellsize = cellsize}

print('\n------------------\nTest01: Create\n------------------')
local robj = geo.geojson(params)
runner.check(robj ~= nil)


print('\n------------------\nTest02: dim\n------------------')
local rows, cols  = robj:dim()
print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
runner.check(rows == 3)
runner.check(cols == 6)


print('\n------------------\nTest03: bbox\n------------------')
local lon_min, lat_min, lon_max, lat_max = robj:bbox()
print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lan_max: %.2f", lon_min, lat_min, lon_max, lat_max))
runner.check(lon_min ~= 0)
runner.check(lat_min ~= 0)
runner.check(lon_max ~= 0)
runner.check(lon_max ~= 0)

print('\n------------------\nTest04: cellsize\n------------------')
local _cellsize = robj:cell()
print(string.format("cellsize: %f", _cellsize))
runner.check(_cellsize == cellsize)

print('\n------------------\nTest05: pixel\n------------------')
local pixelIn = robj:pixel(rows-1, cols-3)
print("pixel in raster check:", pixelIn)
runner.check(pixelIn == true)

pixelIn = robj:pixel(rows-1, cols-1)
print("pixel out of raster check:", pixelIn)
runner.check(pixelIn == false)


-- Clean Up --

-- Report Results --

runner.report()

