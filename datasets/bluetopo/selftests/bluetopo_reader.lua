local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({})

-- Correct values test for different POIs

local lons = {-81.02, -89.66, -94.72}
local lats = { 31.86,  29.99,  29.35}
local height = 0

local expElevation   = {-14.10, -4.28, -17.18}
local expUncertainty = {  2.58,  0.34,  1.32}
local expContributor = { 63846, 24955, 102286}

local elevation_tolerance = 50.0 -- BlueTopo is updated constantly; we just want to check that a valid elevation (within reason) can be sampled

print(string.format("\n--------------------------------\nTest: BlueTopo Correct Values\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", bands = {"Elevation", "Uncertainty", "Contributor"}, sort_by_index = true }))
runner.assert(dem ~= nil, "failed to create bluetopo-bathy raster", true)

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
            local band = v["band"]
            local value = v["value"]
            fname = v["file"]
            print(string.format("(%6.2f, %6.2f)  Band: %11s %8.2f  %s", lon, lat, band, value, fname))

            if band == "Elevation" then
                runner.assert(math.abs(value - expElevation[j]) < elevation_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            elseif band == "Uncertainty" then
                runner.assert(math.abs(value - expUncertainty[j]) < elevation_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            elseif band == "Contributor" then
                runner.assert(value == expContributor[j], string.format("Point: [%d]=%d, (%.3f, %.3f) ======> FAILED", j, value, lon, lat))
            end
        end
        print("\n")
    end
end

-- Report Results --

runner.report()


