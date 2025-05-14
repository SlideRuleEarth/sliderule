local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- Correct values test for different POIs

local lons = {-40, -20, -120}
local lats = { 70, -20,   20}
local height = 0

-- Updataed results for GEBCO 2024
local expDepth = { -64,  -4940, -4072}
local expFlags = {  70,     40,    44}

-- Tolerance for depth values is needed because of the different versions of PROJLIB
-- The values are different in the different versions of the library.
-- We add 1 meter of depth tolerance for the test to pass
local depth_tolerance = 1;

print(string.format("\n--------------------------------\nTest: GEBCO Correct Values default with no band specified\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "gebco-bathy", algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
runner.assert(dem ~= nil)

for j, lon in ipairs(lons) do
    local lat = lats[j]
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            local value = v["value"]
            fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))

            assert( math.abs(value - expDepth[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFlags[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end

print(string.format("\n--------------------------------\nTest: GEBCO Correct Values band=2024\n--------------------------------"))
dem = geo.raster(geo.parms({ asset = "gebco-bathy", bands = {"2024"}, algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
runner.assert(dem ~= nil)

for j, lon in ipairs(lons) do
    local lat = lats[j]
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            local value = v["value"]
            fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))

            assert( math.abs(value - expDepth[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFlags[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end


-- Different results for GEBCO 2023
expDepth = { -64,  -4933, -4072}

print(string.format("\n--------------------------------\nTest: GEBCO Correct Values band=2023\n--------------------------------"))
dem = geo.raster(geo.parms({ asset = "gebco-bathy", bands = {"2023"}, algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
runner.assert(dem ~= nil)

for j, lon in ipairs(lons) do
    local lat = lats[j]
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            local value = v["value"]
            fname = v["file"]
            local flags = v["flags"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))

            assert( math.abs(value - expDepth[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFlags[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end

-- Report Results --

runner.report()


