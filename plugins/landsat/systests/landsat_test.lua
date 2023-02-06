local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local  lon = 180.00
local  lat = 66.34  -- Arctic Circle lat

print(string.format("\n-------------------------------------------------\nLandsat Plugin test\n-------------------------------------------------"))

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}
local demType = "landsat-hls"
local dem = geo.raster(demType, samplingAlgs[i], 0)

local tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read",lon, lat))
else
    local el, fname
    for j, v in ipairs(tbl) do
        el = v["value"]
        fname = v["file"]
    end
    print(string.format("%20s (%02d) %15f", samplingAlgs[0], el, fname))
end

os.exit()
