local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'lpdaac-cloud'})

local geojsonfile = dirpath.."../data/grand_mesa.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Self Test --

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI)\n-------------------------------------------------"))
local demType = "landsat-hls"
local t0str = "2022:01:05:00:00:00"
local t1str = "2022:01:15:00:00:00"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, t0=t0str, t1=t1str, bands = {"NDVI"}, catalog = contents, sort_by_index = true }))
runner.assert(dem ~= nil)

local ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
-- This test ignores lon, lat, lon_incr, lat_incr, pointCount as they are not used.
-- It opens a test file with points.
local pointsFile = dirpath.."../data/grand_mesa_poi.txt"
local pointsInFile = 26183  -- number of points in file
local maxPointCount = 1000  -- number of points to sample, 1000 will trigger all threaded code
status = ut:test(0, 0, 0, 0, maxPointCount, pointsFile);
runner.assert(status, "Failed sampling test")

-- Clean Up --

ut:destroy()

-- Report Results --

runner.report()

