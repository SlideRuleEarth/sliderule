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
local  lon =   -108.1
local  lat =     39.1
local  height =   0.0


print(string.format("\n-------------------------------------------------\nesa worldcover 10meter sample POI\n-------------------------------------------------"))

local expResults = {{10.0, 1309046418, '/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt'}}

local demType = "esa-worldcover-10meter"
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


print(string.format("\n-------------------------------------------------\nesa worldcover 10meter subset AOI\n-------------------------------------------------"))

starttime = time.latch();
tbl, err = dem:subset(-108.3412, 38.8236, -107.7292, 39.1956)
stoptime = time.latch();
print(string.format("subset time: %f", stoptime - starttime))
runner.assert(err == 0)

local subsetCnt = 0
for i, v in ipairs(tbl) do
    local size = v["size"]

    local mbytes = size / (1024*1024)
    print(string.format("(%02d) size: %d (%.2fMB)", i, size, mbytes))
    subsetCnt = subsetCnt + 1

    runner.assert(size > 0)
end
runner.assert(subsetCnt == 1, string.format("Received unexpected number of subsets: %d", subsetCnt))

-- Report Results --

runner.report()
