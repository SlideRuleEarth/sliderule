local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --

-- Unit Test --

lon = -80.87
lat =  32.06

lon_incr = 0.0001
lat_incr = 0.0001
pointCount = 10

print('\n------------------\nTest: RasterSampler::bluetopo-bathy\n------------------')
local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", bands = {"Elevation", "Uncertainty", "Contributor"}, sort_by_index = true }))
runner.check(dem ~= nil)

ut = geo.ut_sample(dem)
runner.check(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.check(status, "Failed sampling test")
ut = nil


-- Clean Up --

-- Report Results --

runner.report()

