local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

-- Correct values test for different POIs

local lons = {-81.02, -89.66, -94.72}
local lats = { 31.86,  29.99,  29.35}
height = 0

local expElevation   = {-14.10, -4.28, -17.18}
local expUncertainty = {  2.58,  0.34,  1.32}
local expContributor = { 63846, 24955, 45641}

local elevation_tolerance = 0.01;

print(string.format("\n--------------------------------\nTest: BlueTopo Correct Values\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", bands = {"Elevation", "Uncertainty", "Contributor"}, sort_by_index = true }))
runner.assert(dem ~= nil)

for j, lon in ipairs(lons) do
    lat = lats[j]
    tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            band = v["band"]
            value = v["value"]
            fname = v["file"]
            print(string.format("(%6.2f, %6.2f)  Band: %11s %8.2f  %s", lon, lat, band, value, fname))

            if band == "Elevation" then
                runner.assert(math.abs(value - expElevation[j]) < elevation_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            elseif band == "Uncertainty" then
                runner.assert(math.abs(value - expUncertainty[j]) < elevation_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            elseif band == "Contributor" then
                runner.assert(value == expContributor[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            end
        end
        print("\n")
    end
end

-- Report Results --

runner.report()


