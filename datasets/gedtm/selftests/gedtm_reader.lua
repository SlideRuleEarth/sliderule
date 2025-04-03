local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local assets = asset.loaddir()

-- local console = require("console")
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Self Test --

local  sigma = 1.0e-9

-- Amazon forest
local  lon =    -62.0
local  lat =     -3.0
local  height =   0.0

print(string.format("\n-------------------------------------------------\nGEDTM 30 meter sample\n-------------------------------------------------"))

local expResults = {{379.0,  1422230418.00, '/vsicurl/https://s3.opengeohub.org/global/edtm/legendtm_rf_30m_m_s_20000101_20231231_go_epsg.4326_v20250130.tif'}}
local demType = "gedtm-30meter"
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
    print(string.format("%.2f time: %.2f %s", el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))


print(string.format("\n-------------------------------------------------\nGEDTM Standard Deviation sample\n-------------------------------------------------"))

expResults = {{74.0,  1423094418.00, '/vsicurl/https://s3.opengeohub.org/global/edtm/gendtm_rf_30m_std_s_20000101_20231231_go_epsg.4326_v20250209.tif'}}
demType = "gedtm-std"
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
starttime = time.latch();
tbl, err = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("%.2f time: %.2f %s", el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))



print(string.format("\n-------------------------------------------------\nGEDTM Distance From Mean sample\n-------------------------------------------------"))

expResults = {{16.0,  1419552018.00, '/vsicurl/https://s3.opengeohub.org/global/dtm/v3/dfme_edtm_m_30m_s_20000101_20221231_go_epsg.4326_v20241230.tif'}}
demType = "gedtm-dfm"
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
starttime = time.latch();
tbl, err = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("%.2f time: %.2f %s", el, time, fname))
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
