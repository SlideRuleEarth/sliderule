local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local lon = -178.0
local lat =   51.7
local height = 0


print('Memory test')

local dem = geo.raster("arcticdem-mosaic", "NearestNeighbour", 0)
local tbl, err = dem:sample(lon, lat, height)

if err ~= 0 then
    failedSamples = failedSamples + 1
    print(string.format("Point (%.3f, %.3f) ======> FAILED to read",lon, lat))
else
     for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %14f %s", j, el, fname))
     end
end

local dem = geo.raster("arcticdem-strips", "NearestNeighbour", 0)
local tbl, err = dem:sample(lon, lat, height)

if err ~= 0 then
    failedSamples = failedSamples + 1
    print(string.format("Point (%.3f, %.3f) ======> FAILED to read",lon, lat))
else
     for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %14f %s", j, el, fname))
     end
end

sys.quit()
