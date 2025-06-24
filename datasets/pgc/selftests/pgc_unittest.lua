local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/rema_strips.geojson"
local contents = readgeojson.load(jsonfile)

-- Self Test --

print('\n------------------\nTest01 RasterSubset::rema-mosaic \n------------------')

local demType = "rema-mosaic"
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=50, zonal_stats=true}))
runner.assert(dem ~= nil)
local ut = geo.ut_subset(dem)
runner.assert(ut ~= nil)
local status = ut:test()
runner.assert(status, "Failed subset test")
ut = nil


print('\n------------------\nTest02 RasterSubset::rema-strips \n------------------')
demType = "rema-strips"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=50, zonal_stats=true}))
runner.assert(dem ~= nil)
ut = geo.ut_subset(dem)
runner.assert(ut ~= nil)
status = ut:test()
runner.assert(status, "Failed subset test")
ut = nil


lon = -150
lat =   70
lon_incr = 0.0001
lat_incr = 0.0001
pointCount = 10

print('\n------------------\nTest03 RasterSampler::arcticdem-mosaic\n------------------')
demType = "arcticdem-mosaic"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=false}))
runner.assert(dem ~= nil)

ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.assert(status, "Failed sampling test")
ut = nil


readgeojson = require("readgeojson")
jsonfile = td .. "../data/arcticdem_strips.geojson"
contents = readgeojson.load(jsonfile)

-- Use temporal filter for range t0 to t1 to limit number of strips per point
local t0str = "2014:2:25:23:00:00"
local t1str = "2018:6:2:24:00:00"
pointCount = 2

print('\n------------------\nTest04 RasterSampler::arcticdem-strips\n------------------')
demType = "arcticdem-strips"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, t0=t0str, t1=t1str, sort_by_index=true}))
runner.assert(dem ~= nil)

ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.assert(status, "Failed sampling test")
ut = nil

-- Clean Up --

-- Report Results --

runner.report()
