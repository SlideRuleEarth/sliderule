local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local lon = -178.0
local lat =   51.7


print('Memory test')

local dem = geo.raster("arcticdem-mosaic", "NearestNeighbour", 0)
local tbl, status = dem:sample(lon, lat)

if status ~= true then
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
local tbl, status = dem:sample(lon, lat)

if status ~= true then
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
