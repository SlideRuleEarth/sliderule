local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local lon = -80.0   -- DO NOT CHANGE lon and lat, later tests are hardcoded to these values!!!
local lat = -80.0
local height = 0

local demTypes = {"rema-mosaic", "rema-strips"}
local demTypeCnt = 2

for i = 1, demTypeCnt do

    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0}))
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s sample\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat, height)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local sampleCnt = 0

    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %8.2f %s", i, el, fname))
        runner.check(el ~= -1000000)  --INVALID_SAMPLE_VALUE from VrtRaster.h
        runner.check(string.len(fname) > 0)
        sampleCnt = sampleCnt + 1
    end

    if demType == "rema-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 10)
    end

    print(string.format("\n--------------------------------\nTest: %s dim\n--------------------------------", demType))
    local rows, cols = dem:dim()
    print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
    runner.check(rows ~= 0)
    runner.check(cols ~= 0)

    print(string.format("\n--------------------------------\nTest: %s bbox\n--------------------------------", demType))
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lan_max: %.2f", lon_min, lat_min, lon_max, lat_max))
    runner.check(lon_min ~= 0)
    runner.check(lat_min ~= 0)
    runner.check(lon_max ~= 0)
    runner.check(lon_max ~= 0)

    print(string.format("\n--------------------------------\nTest: %s cellsize\n--------------------------------", demType))
    local cellsize = dem:cell()
    print(string.format("cellsize: %d", cellsize))

    if demType == "rema-mosaic" then
        -- runner.check(cellsize == 2.0)
    else
        runner.check(cellsize == 0.0)
    end

end

for i = 1, demTypeCnt do

    local demType = demTypes[i];
    local samplingRadius = 30
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true, with_flags=true}))

    runner.check(dem ~= nil)

    print(string.format("\n--------------------------------\nTest: %s Zonal Stats with qmask\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat, height)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local sampleCnt = 0

    local el, cnt, min, max, mean, median, stdev, mad, flags
    for j, v in ipairs(tbl) do
        el = v["value"]
        cnt = v["count"]
        min = v["min"]
        max = v["max"]
        mean = v["mean"]
        median = v["median"]
        stdev = v["stdev"]
        mad = v["mad"]
        flags = v["flags"]

        if el ~= -9999.0 then
            print(string.format("(%02d) %6.2fm   cnt: %03d   qmask: 0x%x   min: %6.2f   max: %6.2f   mean: %6.2f   median: %6.2f   stdev: %6.2f   mad: %6.2f", j, el, cnt, flags, min, max, mean, median, stdev, mad))
            runner.check(el ~= 0.0)
            runner.check(min <= el)
            runner.check(max >= el)
            runner.check(mean ~= 0.0)
            runner.check(stdev ~= 0.0)
        end
        sampleCnt = sampleCnt + 1
    end

    if demType == "rema-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 10)
    end
end


-- Correct values test for different POIs

local lons = {-80,   0.5,  -170,  80}
local lats = {-80, -81.0,   -85, -85}
height = 0

local expResultsMosaic = {328.0156250,  2271.6250000, 1341.6015625, 3408.6953125}
local expResultsStrips = {328.2968750,  2282.1406250, 1349.5468750, 3409.8359375}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {10, 4, 6, 6}

for i = 1, 2 do
    local isMosaic = (i == 1)

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Reading Correct Values\n--------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

    for j, lon in ipairs(lons) do
        local sampleCnt = 0
        lat = lats[j]
        tbl, status = dem:sample(lon, lat, height)
        if status ~= true then
            print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",j, lon, lat))
        else
            local el, fname
            for k, v in ipairs(tbl) do
                el = v["value"]
                fname = v["file"]
                print(string.format("(%02d)   (%6.1f, %5.1f) %16.7fm   %s", k, lon, lat, el, fname))
                sampleCnt = sampleCnt + 1

                if k == 1 then -- Check all mosaics but only first strip for each POI
                    if isMosaic then
                        expected_value = expResultsMosaic[j]
                    else
                        expected_value = expResultsStrips[j]
                    end
                    -- print(string.format("(%02d) value: %16.7f  exp: %16.7f", k, el, expected_value))
                    expected_max = expected_value + 0.0000001
                    expected_min = expected_value - 0.0000001
                    runner.check(el <= expected_max and el >= expected_min)
                end
            end
        end

        if isMosaic == true then
            expectedSamplesCnt = 1
        else
            expectedSamplesCnt = expSamplesCnt[j]
            print("\n")
        end
        -- print(string.format("(%02d) value: %d  exp: %d", i, sampleCnt, expectedSamplesCnt))
        runner.check(sampleCnt == expectedSamplesCnt)
    end
end

-- Report Results --

runner.report()

