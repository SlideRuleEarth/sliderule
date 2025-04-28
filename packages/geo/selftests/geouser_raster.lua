local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local base64 = require("base64")

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local raster = dirpath.."../data/geouser_test_raster.tif"

-- Self Test --

local f = io.open(raster, "rb")
local rasterfile = nil

if f ~= nil then
    rasterfile = f:read("*a")
    f:close()
else
    runner.assert(false, "failed to open raster file")
end

local encodedRaster = base64.encode( rasterfile )
local len = string.len(encodedRaster)
local params = {data = encodedRaster, length = len, date = 0, elevation = true}

local robj = geo.userraster(params):name("userraster")
runner.assert(robj ~= nil)


print('\n------------------\nTest01: sample\n------------------')
local lon = 149.00001
local lat =  69.00001
local height = 0
local tbl, err = robj:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

local el, file
if tbl ~= nil then
    for j, v in ipairs(tbl) do
        s = v["value"]
        fname = v["file"]
    end
    print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
    runner.assert(math.abs(10 - s) < 0.01)
end


print('\n------------------\nTest02: dim\n------------------')
local rows, cols  = robj:dim()
print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
runner.assert(rows == 12)
runner.assert(cols == 12012)


print('\n------------------\nTest03: bbox\n------------------')
local lon_min, lat_min, lon_max, lat_max = robj:bbox()
print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lan_max: %.2f", lon_min, lat_min, lon_max, lat_max))
runner.assert(lon_min ~= 0)
runner.assert(lat_min ~= 0)
runner.assert(lon_max ~= 0)
runner.assert(lon_max ~= 0)

print('\n------------------\nTest04: cellsize\n------------------')
local cellsize = robj:cell()
print(string.format("cellsize: %f", cellsize))
runner.assert(math.abs(0.000083 - cellsize) < 0.000001)

-- Clean Up --

-- Report Results --

runner.report()
