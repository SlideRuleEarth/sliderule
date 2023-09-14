local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local sigma = 1.0e-9

local lon = -80.0
local lat = -80.0
local height = 0

local demTypes = {"rema-mosaic", "rema-strips"}

for i = 1, #demTypes do
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
        sampleCnt = sampleCnt + 1
    end

    print(string.format("\n--------------------------------\nTest: %s dim\n--------------------------------", demType))
    local rows, cols = dem:dim()
    print(string.format("dimensions: rows: %d, cols: %d", rows, cols))

    print(string.format("\n--------------------------------\nTest: %s bbox\n--------------------------------", demType))
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print(string.format("lon_min: %.12f, lat_min: %.12f, lon_max: %.12f, lat_max: %.12f", lon_min, lat_min, lon_max, lat_max))

    print(string.format("\n--------------------------------\nTest: %s cellsize\n--------------------------------", demType))
    local cellsize = dem:cell()
    print(string.format("cellsize: %d", cellsize))

    if demType == "rema-mosaic" then
        runner.check(rows == 2921100)
        runner.check(cols == 2725100)
        runner.check(lon_min == -2700100.0)
        runner.check(lat_min == -2500100.0)
        runner.check(lon_max ==  2750100.0)
        runner.check(lat_max ==  3342100.0)
        runner.check(cellsize == 2.0)
    else
        runner.check(rows == 512)
        runner.check(cols == 512)
        runner.check(math.abs(lon_min - -80.469941394992) < sigma)
        runner.check(math.abs(lat_min - -80.037772220485) < sigma)
        runner.check(math.abs(lon_max - -78.624522758183) < sigma)
        runner.check(math.abs(lat_max - -78.917647972358) < sigma)
        runner.check(cellsize == 0.0)
    end
end

-- Correct values test for different POIs

local lons = {-80,   0.5,  -170,  80}
local lats = {-80, -81.0,   -85, -85}
height = 0

local expResultsMosaic = {328.0156250,  2271.6250000, 1341.6015625, 3408.6953125}
local expResultsStrips = {328.2968750,  2282.1406250, 1349.5468750, 3409.8359375}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {10, 4, 6, 6}

for i = 1, #demTypes do
    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Correct Values\n--------------------------------", demType))
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
                    if demType == "rema-mosaic" then
                        expected_value = expResultsMosaic[j]
                    else
                        expected_value = expResultsStrips[j]
                    end
                    -- print(string.format("(%02d) value: %16.7f  exp: %16.7f", k, el, expected_value))
                    expected_max = expected_value + 0.0000001
                    expected_min = expected_value - 0.0000001
                    runner.check(el <= expected_max and el >= expected_min)
                else
                    runner.check(el > 300)  --All others
                end
            end
        end

        if demType == "rema-mosaic" then
            expectedSamplesCnt = 1
        else
            expectedSamplesCnt = expSamplesCnt[j]
            print("\n")
        end
        -- print(string.format("(%02d) value: %d  exp: %d", i, sampleCnt, expectedSamplesCnt))
        runner.check(sampleCnt == expectedSamplesCnt)
    end
end

--One sample run with extent limit
lons = {-170}
lats = { -85}
height = 0

local extentbox = {-175, -90, -165, -80}

local expResultsMosaic = {1341.6015625}
local expResultsStrips = {1349.5468750}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {6}

for i = 1, #demTypes do
    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Reading Correct Values with AOI box\n--------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", aoi_bbox=extentbox}))

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
                    if demType == "rema-mosaic" then
                        expElevation = expResultsMosaic[j]
                    else
                        expElevation = expResultsStrips[j]
                    end
                    runner.check(math.abs(el - expElevation) < sigma)
                else
                    runner.check(el > 1340)  --All others must at least be 1340
                end
            end
        end

        if demType == "rema-mosaic" then
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

