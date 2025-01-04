local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

-- Correct values test for different POIs

local sigma = 1.0e-9

local lons = {-150, -40, 100, 150}
local lats = {  70,  70,  70,  75}
local heights = {0,   0,   0,   0}

local expResultsMosaic = { 116.250000000,  2968.085937500, 475.742187500, 19.890625000}
local expResultsStrips = { 119.554687500,  2968.015625000, 474.156250000, 10.296875000}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {8, 8, 4, 14}

for i = 1, #demTypes do

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Correct Values\n--------------------------------", demType))
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", sort_by_index=true}))
    local tbl, err = dem:batchsample(lons, lats, heights)

    if err ~= 0 then
        print("FAILED to batchsample")
        runner.check(false)
    else
        for j, pointSamples in ipairs(tbl) do
            local lon, lat = lons[j], lats[j]
            local sampleCnt = #pointSamples
            print(string.format("Point: %d, (%.3f, %.3f) Sample Count: %d", j, lon, lat, sampleCnt))

            if sampleCnt > 0 then
                for k, sample in ipairs(pointSamples) do
                    local el = sample["value"]
                    local fname = sample["file"]
                    print(string.format("    Sample (%02d): %16.9fm   %s", k, el, fname))

                    if k == 1 then -- Check all mosaics but only first strip for each POI
                        local expElevation
                        if demType == "arcticdem-mosaic" then
                            expElevation = expResultsMosaic[j]
                        else
                            expElevation = expResultsStrips[j]
                        end
                        runner.check(math.abs(el - expElevation) < sigma)
                    else
                        runner.check(el > 10)  -- Check all other samples
                    end
                end
            else
                print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read samples", j, lon, lat))
            end

            local expectedSamplesCnt
            if demType == "arcticdem-mosaic" then
                expectedSamplesCnt = 1
            else
                expectedSamplesCnt = expSamplesCnt[j]
            end
            runner.check(sampleCnt == expectedSamplesCnt)
        end
    end
end

-- Report Results --

runner.report()

