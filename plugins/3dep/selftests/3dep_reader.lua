local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")
local _,td = runner.srcscript()

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  lon = -108.1
local  lat = 39.1

print(string.format("\n-------------------------------------------------\n3dep 1meter DEM test\n-------------------------------------------------"))

local demType = "3dep-1meter-dem"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))


local sampleCnt = 0
local sampledValue = 0
local gpsTime = 0
local tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        time  = v["time"]

        sampleCnt = sampleCnt + 1
        if sampleCnt == 1 then
            sampledValue = value
            gpsTime = time
        end

        print(string.format("(%02d) value %10.6f, time %.2f, fname: %s", j, value, time, fname))
        runner.check(time ~= 0.0)
    end
end

runner.check(sampleCnt == 4)

local expected_value = 2654.263916 -- first sample from the group of samples
local expected_max = expected_value + 0.00001
local expected_min = expected_value - 0.00001

runner.check(sampledValue <= expected_max and sampledValue >= expected_min)
runner.check(gpsTime == 1289671725.0)
dem = nil



-- Report Results --

runner.report()

