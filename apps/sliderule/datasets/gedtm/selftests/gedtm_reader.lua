local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({})

-- tolerance
local  sigma = 1.0e-9

-- Amazon forest
local  lon =    -62.0
local  lat =     -3.0
local  height =   0.0

-- location of data
local path = '/vsis3/' .. sys.getcfg("project_bucket") .. '/data/GEDTM/'

-- Self Test --

runner.unittest("GEDTM 30 meter sample", function()

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

end)


runner.unittest("GEDTM standard deviation sample", function()

    local expResults = {{74.0,  1423094418.00, path .. 'gendtm_rf_30m_std_s_20000101_20231231_go_epsg.4326_v20250209.tif'}}
    local demType = "gedtm-std"
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

end)


runner.unittest("GEDTM distance from mean sample", function()

    local expResults = {{16.0,  1419552018.00, path .. 'dfme_edtm_m_30m_s_20000101_20221231_go_epsg.4326_v20241230.tif'}}
    local demType = "gedtm-dfm"
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

end)

-- Report Results --

runner.report()
