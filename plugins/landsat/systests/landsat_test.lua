local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local  lon = -179.0
local  lat = 51.0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test\n-------------------------------------------------"))

local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0 }))

for i = 1, 1, 1 do
    local tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read",lon, lat))
    else
        local el, fname
        for j, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
        end
        print(string.format("%15f, fname", el, fname))
    end

    lon = lon + 0.1
    -- lat = lat + 0.1
end

os.exit()
