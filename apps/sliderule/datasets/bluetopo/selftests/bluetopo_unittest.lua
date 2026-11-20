local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Self Test --

runner.unittest("RasterSampler::bluetopo-bathy", function()

    local lon = -80.87
    local lat =  32.06

    local lon_incr = 0.0001
    local lat_incr = 0.0001
    local pointCount = 10

    local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", bands = {"Elevation", "Uncertainty", "Contributor"}, sort_by_index = true }))
    runner.assert(dem ~= nil)

    local ut = geo.ut_sample(dem)
    runner.assert(ut ~= nil)

    local status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
    runner.assert(status, "Failed sampling test")

end)

-- Report Results --

runner.report()

