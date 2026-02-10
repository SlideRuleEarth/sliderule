local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Self Test --

runner.unittest("3DEP Unit Test", function()

    local geojsonfile = dirpath.."../data/grand_mesa_1m_dem.geojson"
    local f = io.open(geojsonfile, "r")
    assert(f)
    local contents = f:read("*all")
    f:close()

    local demType = "usgs3dep-1meter-dem"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents, sort_by_index = true }))
    runner.assert(dem ~= nil)

    local ut = geo.ut_sample(dem)
    runner.assert(ut ~= nil)

    -- This test ignores lon, lat, lon_incr, lat_incr, pointCount as they are not used.
    -- It opens a test file with points.
    local pointsFile = dirpath.."../../landsat/data/grand_mesa_poi.txt"
    local maxPointCount = 110
    local status = ut:test(0, 0, 0, 0, maxPointCount, pointsFile);
    runner.assert(status, "Failed sampling test")

end)

-- Report Results --

runner.report()

