local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Setup --

-- runner.log(core.DEBUG)

local geojsonfile = dirpath.."../data/grandmesa.geojson"

-- Self Test --

local f = io.open(geojsonfile, "r")
local vectorfile = nil

if f ~= nil then
    vectorfile = f:read("*a")
    f:close()
else
    runner.assert(false, "failed to open geojson file")
end

local cellsize = 0.01
local robj = geo.geojson(vectorfile, cellsize)
runner.assert(robj ~= nil)


print('\n------------------\nTest01: sample\n------------------')
local lon = -108
local lat =   39
local height = 0
local tbl, err = robj:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

local el, file
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.assert(s == 1)


print('\n------------------\nTest02: dim\n------------------')
local rows, cols  = robj:dim()
print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
runner.assert(rows == 37)
runner.assert(cols == 61)


print('\n------------------\nTest03: bbox\n------------------')
local lon_min, lat_min, lon_max, lat_max = robj:bbox()
print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lan_max: %.2f", lon_min, lat_min, lon_max, lat_max))
runner.assert(lon_min ~= 0)
runner.assert(lat_min ~= 0)
runner.assert(lon_max ~= 0)
runner.assert(lon_max ~= 0)

print('\n------------------\nTest04: cellsize\n------------------')
local _cellsize = robj:cell()
print(string.format("cellsize: %f", _cellsize))
runner.assert(_cellsize == cellsize)

-- Edge of bbox
print('\n------------------\nTest05: edge of bbox\n------------------')
lon = -108.34
lat =   38.90
tbl, err = robj:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)
for j, v in ipairs(tbl) do
    s = v["value"]
    fname = v["file"]
end
print(string.format("sample at lon: %.2f, lat: %.2f is %.2f, %s", lon, lat, s, fname))
runner.assert(tostring(s) == "nan")


-- Outside of bbox
print('\n------------------\nTest06: outside bbox\n------------------')
lon = -100
lat =   40
tbl, err = robj:sample(lon, lat, height)
runner.assert(err ~= 0)
runner.assert(#tbl == 0)

-- Clean Up --

-- Report Results --

runner.report()
