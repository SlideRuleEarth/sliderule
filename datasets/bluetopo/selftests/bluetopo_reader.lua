local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

-- Correct values test for different POIs

local lons = {-80.87, -81.02, -89.66, -94.72}
local lats = { 32.06,  31.86,  29.99,  29.35}
height = 0

local expDepth = { -2.02, -14.03, -4.28, -17.18}
local expFlags = {  3.32}

-- Tolerance for depth values is needed because of the different versions of PROJLIB
-- The values are different in the different versions of the library.
-- We add 1 meter of depth tolerance for the test to pass
local depth_tolerance = 1;

print(string.format("\n--------------------------------\nTest: BlueTopo Correct Values\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "bluetopo-bathy", algorithm = "NearestNeighbour", with_flags=true, sort_by_index = true }))
runner.check(dem ~= nil)

for j, lon in ipairs(lons) do
    lat = lats[j]
    tbl, err = dem:sample(lon, lat, height)
    runner.check(err == 0)
    runner.check(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read, err# %d",j, lon, lat, err))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            value = v["value"]
            fname = v["file"]
            flags = v["flags"]
            print(string.format("(%02d)   (%6.2f, %6.2f) %6.2fm  %4.2f  %s", k, lon, lat, value, flags, fname))

            assert( math.abs(value - expDepth[j]) < depth_tolerance, string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            -- assert(flags == expFlags[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end

-- Report Results --

runner.report()


