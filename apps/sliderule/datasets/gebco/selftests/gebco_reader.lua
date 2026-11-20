local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({})

-- correct values test for different POIs
local lons = {-40, -20, -120}
local lats = { 70, -20,   20}
local height = 0

-- expected results for GEBCO
local expDepth2024 = { -64,  -4940, -4072}
local expFalgs2023 = {  70,     40,    44}
local expDepth2023 = { -64,  -4933, -4072}

-- tolerance for depth values is needed because of the different versions of PROJLIB
-- the values are different in the different versions of the library.
-- we add 1 meter of depth tolerance for the test to pass
local depth_tolerance = 1;

-- Self Tests --

runner.unittest("GEBCO Correct Values", function() -- defaults with no band specified
    local dem = geo.raster(geo.parms({ asset = "gebco-s3", algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
    runner.assert(dem ~= nil, "failed to create dem", true)
    for j, lon in ipairs(lons) do
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err), true)
        runner.assert(tbl ~= nil, "failed to return samples", true)
        for k, v in ipairs(tbl) do
            local value = v["value"]
            local fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))
            assert(math.abs(value - expDepth2024[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFalgs2023[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end)

runner.unittest("GEBCO Correct Values (band=2024)", function()
    local dem = geo.raster(geo.parms({ asset = "gebco-s3", bands = {"2024"}, algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
    runner.assert(dem ~= nil, "failed to create dem", true)
    for j, lon in ipairs(lons) do
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err), true)
        runner.assert(tbl ~= nil, "failed to return samples", true)
        for k, v in ipairs(tbl) do
            local value = v["value"]
            local fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))
            assert(math.abs(value - expDepth2024[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFalgs2023[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end)

runner.unittest("GEBCO Correct Values (band=2023)", function()
    local dem = geo.raster(geo.parms({ asset = "gebco-s3", bands = {"2023"}, algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
    runner.assert(dem ~= nil, "failed to create dem", true)
    for j, lon in ipairs(lons) do
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err), true)
        runner.assert(tbl ~= nil, "failed to return samples", true)
        for k, v in ipairs(tbl) do
            local value = v["value"]
            local fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))
            assert(math.abs(value - expDepth2023[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFalgs2023[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end)

-- Report Results --

runner.report()
