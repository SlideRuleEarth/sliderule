local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local assets = asset.loaddir()
local role_auth_script = core.script("iam_role_auth")
while not aws.csget("iam-role") do
    print("Waiting to establish IAM role...")
    sys.wait(1)
end

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Self Test --

local  sigma = 1.0e-9

-- Amazon forest
local  lon =    -62.0
local  lat =     -3.0
local  height =   0.0

local path = '/vsis3/sliderule/data/GEDTM/'

print(string.format("\n-------------------------------------------------\nGEDTM 30 meter sample\n-------------------------------------------------"))

local expResults = {{379.0,  1422230418.00, path .. 'legendtm_rf_30m_m_s_20000101_20231231_go_epsg.4326_v20250130.tif'}}
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

expResults = {{74.0,  1423094418.00, path .. 'gendtm_rf_30m_std_s_20000101_20231231_go_epsg.4326_v20250209.tif'}}
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

expResults = {{16.0,  1419552018.00, path .. 'dfme_edtm_m_30m_s_20000101_20221231_go_epsg.4326_v20241230.tif'}}
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
