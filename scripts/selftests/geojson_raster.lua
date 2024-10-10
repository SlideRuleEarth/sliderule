local runner = require("test_executive")
console = require("console")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

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

local cellsize = 0.01
local robj = geo.geojson(vectorfile, cellsize)
runner.check(robj ~= nil)


print('\n------------------\nTest01: sample\n------------------')
local lon = -108
local lat =   39
local height = 0
local tbl, err = robj:sample(lon, lat, height)
runner.check(err == 0)
runner.check(tbl ~= nil)

local el, file
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.check(s == 1)


print('\n------------------\nTest02: dim\n------------------')
local rows, cols  = robj:dim()
print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
runner.check(rows == 37)
runner.check(cols == 61)


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

-- Edge of bbox
print('\n------------------\nTest05: edge of bbox\n------------------')
lon = -108.34
lat =   38.90
tbl, err = robj:sample(lon, lat, height)
runner.check(err == 0)
runner.check(tbl ~= nil)
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.check(tostring(s) == "nan")


-- Outside of bbox
print('\n------------------\nTest06: outside bbox\n------------------')
lon = -100
lat =   40
tbl, err = robj:sample(lon, lat, height)
runner.check(err ~= 0)
runner.check(#tbl == 0)


-- Clean Up --

-- Report Results --

runner.report()

