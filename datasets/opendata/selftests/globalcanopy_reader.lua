local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local assets = asset.loaddir()

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Self Test --

local  sigma = 1.0e-9

-- Amazon forest
local  lon =    -62.0
local  lat =     -3.0
local  height =   0.0

print(string.format("\n-------------------------------------------------\nMeta (\u{221E}) global canopy height sample POI\n-------------------------------------------------"))

local expResults = {{20.0, 1396483218, '/vsis3/sliderule/data/GLOBALCANOPY/META_GlobalCanopyHeight_1m_2024_v1.vrt'}}

local demType = "meta-globalcanopy-1meter"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))

-- Report Results --

runner.report()
