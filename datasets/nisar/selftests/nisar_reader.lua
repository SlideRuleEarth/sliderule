local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({"asf-cloud"})
runner.log(core.DEBUG)

-- TODO: This index file is from landsat hls but for now it will trick the reader into thinking it is from NISAR
--       Create a small geojson file with a few hdf5 files which would look like what CMR would return for NISAR query
local srcfile, dirpath = runner.srcscript()
local geojsonfile = dirpath.."../../landsat/data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file")
if not f then return end
local contents = f:read("*all")
f:close()

-- Correct values test for different POIs

local lons = {-116.9985}
local lats = {   0.0011}
local height = 0

local expElevation   = {-14.10, -4.28, -17.18}
local expUncertainty = {  2.58,  0.34,  1.32}
local expContributor = { 63846, 24955, 45641}

local elevation_tolerance = 50.0 -- BlueTopo is updated constantly; we just want to check that a valid elevation (within reason) can be sampled

print(string.format("\n--------------------------------\nTest: NISAR L2 Correct Values\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "nisar-L2-geoff", algorithm = "NearestNeighbour", bands = {"azimuthOffset", "rangeOffset"}, catalog = contents, sort_by_index = true }))
runner.assert(dem ~= nil, "failed to create nisar-L2 dataset", true)

for j, lon in ipairs(lons) do
    local lat = lats[j]
    local tbl, err = dem:sample(lon, lat, height)
--[[
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
                runner.assert(value == expContributor[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            end
        end
        print("\n")
    end
--]]
end

-- Report Results --

runner.report()


