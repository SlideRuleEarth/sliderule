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

print('\n------------------\nTest05: sample\n------------------')
local lon = -108
local lat =   39
local tbl, status = robj:sample(lon, lat)
runner.check(status == true)
runner.check(tbl ~= nil)

local el, file
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.check(s == 1)

-- Edge of bbox
lon = -108.34
lat =   38.90
tbl, status = robj:sample(lon, lat)
runner.check(status == true)
runner.check(tbl ~= nil)
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.check(s == 1)


-- Outside of bbox
lon = -100
lat =   40
tbl, status = robj:sample(lon, lat)
runner.check(status == nil)
runner.check(tbl == nil)


-- Clean Up --

-- Report Results --

runner.report()

