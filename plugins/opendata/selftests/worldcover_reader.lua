local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/grand_mesa_10m_worldcover.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  sigma = 1.0e-9
local  lon =    -108.1
local  lat =      39.1
local  height = 2630.0


print(string.format("\n-------------------------------------------------\nesa worldcover 10meter\n-------------------------------------------------"))

local expResults = {{20.0, 1309046423000, '/vsis3/esa-worldcover/v100/2020/ESA_WorldCover_10m_2020_v100_Map_AWS.vrt'}}

local demType = "esa-worldcover-10meter"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))
local tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.check(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.check(time == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)

-- Report Results --

runner.report()

