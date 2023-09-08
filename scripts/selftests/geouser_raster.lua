local runner = require("test_executive")
console = require("console")
local td = runner.rootdir(arg[0])
local base64 = require("base64")

-- Setup --
console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

local raster = td.."/geouser_test_raster.tif"

-- Unit Test --

local f = io.open(raster, "rb")
local rasterfile = nil

if f ~= nil then
    rasterfile = f:read("*a")
    f:close()
else
    runner.check(false, "failed to open raster file")
end

local encodedRaster = base64.encode( rasterfile )
local len = string.len(encodedRaster)
local params = {data = encodedRaster, length = len, date = 0, elevation = true}

local robj = geo.userraster(params)
runner.check(robj ~= nil)


print('\n------------------\nTest01: sample\n------------------')
local lon = 149.00001
local lat =  69.00001
local height = 0
local tbl, status = robj:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

local el, file
if tbl ~= nil then
    for j, v in ipairs(tbl) do
        s = v["value"]
        fname = v["file"]
    end
    print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
    runner.check(math.abs(10 - s) < 0.01)
end


print('\n------------------\nTest02: dim\n------------------')
local rows, cols  = robj:dim()
print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
runner.check(rows == 12)
runner.check(cols == 12012)


print('\n------------------\nTest03: bbox\n------------------')
local lon_min, lat_min, lon_max, lat_max = robj:bbox()
print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lan_max: %.2f", lon_min, lat_min, lon_max, lat_max))
runner.check(lon_min ~= 0)
runner.check(lat_min ~= 0)
runner.check(lon_max ~= 0)
runner.check(lon_max ~= 0)

print('\n------------------\nTest04: cellsize\n------------------')
local cellsize = robj:cell()
print(string.format("cellsize: %f", cellsize))
runner.check(math.abs(0.000083 - cellsize) < 0.000001)

-- Clean Up --

-- Report Results --

runner.report()

