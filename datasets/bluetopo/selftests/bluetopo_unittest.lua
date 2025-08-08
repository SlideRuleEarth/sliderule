local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Self Test --

lon = -80.87
lat =  32.06

lon_incr = 0.0001
lat_incr = 0.0001
pointCount = 10

print('\n------------------\nTest: RasterSampler::bluetopo-bathy\n------------------')
local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", bands = {"Elevation", "Uncertainty", "Contributor"}, sort_by_index = true }))
runner.assert(dem ~= nil)

ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.assert(status, "Failed sampling test")
ut = nil


-- Clean Up --

-- Report Results --

runner.report()

