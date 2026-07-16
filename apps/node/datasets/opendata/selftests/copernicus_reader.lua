local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Self Test --

runner.unittest("ESA Copernicus 30meter sample POI", function()

    local sigma = 1.0e-9
    local lon = -108.1
    local lat = 39.1
    local height = 0.0

    local expResults = {{2637.891093719017, 1386633618, "/vsis3/raster/COP30/COP30_hh.vrt"}}

    local demType = "esa-copernicus-30meter"
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
        local sampleTime = v["time"]
        print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, sampleTime, fname))
        sampleCnt = sampleCnt + 1

        if tostring(el) ~= "nan" then
            runner.assert(math.abs(el - expResults[i][1]) < sigma)
        end
        runner.assert(sampleTime == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
    runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))

end)

-- Report Results --

runner.report()
