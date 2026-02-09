local runner = require("test_executive")
local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local readgeojson = require("readgeojson")
local rema_contents = readgeojson.load(td .. "../data/rema_strips.geojson")
local arctidem_contents = readgeojson.load(td .. "../data/arcticdem_strips.geojson")

local lon = -150
local lat =   70
local lon_incr = 0.0001
local lat_incr = 0.0001

-- Self Test --

runner.unittest("RasterSubset::rema-mosaic", function()
    local demType = "rema-mosaic"
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=rema_contents, radius=50, zonal_stats=true}))
    runner.assert(dem ~= nil)
    local ut = geo.ut_subset(dem)
    runner.assert(ut ~= nil)
    local status = ut:test()
    runner.assert(status, "Failed subset test")
end)

runner.unittest("RasterSubset::rema-mosstripsaic", function()
    local demType = "rema-strips"
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=rema_contents, radius=50, zonal_stats=true}))
    runner.assert(dem ~= nil)
    local ut = geo.ut_subset(dem)
    runner.assert(ut ~= nil)
    local status = ut:test()
    runner.assert(status, "Failed subset test")
end)

runner.unittest("RasterSampler::arcticdem-mosaic", function()
    local pointCount = 10
    local demType = "arcticdem-mosaic"
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=false}))
    runner.assert(dem ~= nil)
    local ut = geo.ut_sample(dem)
    runner.assert(ut ~= nil)
    local status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
    runner.assert(status, "Failed sampling test")
end)

runner.unittest("RasterSampler::arcticdem-strips", function()
    -- Use temporal filter for range t0 to t1 to limit number of strips per point
    local t0str = "2014:2:25:23:00:00"
    local t1str = "2018:6:2:24:00:00"
    local pointCount = 2
    local demType = "arcticdem-strips"
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=arctidem_contents, radius=0, with_flags=true, t0=t0str, t1=t1str, sort_by_index=true}))
    runner.assert(dem ~= nil)
    local ut = geo.ut_sample(dem)
    runner.assert(ut ~= nil)
    local status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
    runner.assert(status, "Failed sampling test")
end)

-- Report Results --

runner.report()
