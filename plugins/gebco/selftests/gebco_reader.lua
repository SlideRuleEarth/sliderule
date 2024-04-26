local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

-- Correct values test for different POIs

local lons = {-40, -20, -120}
local lats = { 70, -20,   20}
height = 0

local expDepth = { -64,  -4933, -4072}
local expFlags = {  70,     40,    44}

print(string.format("\n--------------------------------\nTest: GEBCO Correct Values\n--------------------------------"))
local dem = geo.raster(geo.parms({ asset = "gebco-bathy", algorithm = "NearestNeighbour", with_flags=true}))
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
            print(string.format("(%02d)   (%6.1f, %5.1f) %8.1fm  %02d  %s", k, lon, lat, value, flags, fname))

            assert(value == expDepth[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
            assert(flags == expFlags[j], string.format("Point: %d, (%.3f, %.3f) ======> FAILED",j, lon, lat))
        end
    end
end

-- Report Results --

runner.report()


