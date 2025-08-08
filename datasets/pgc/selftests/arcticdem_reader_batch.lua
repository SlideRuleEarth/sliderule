local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/arcticdem_strips.geojson"
local contents = readgeojson.load(jsonfile)

-- Self Test --

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

-- Correct values test for different POIs

local sigma = 1.0e-9
local lons = {-150, -149.95}
local lats = {  70,   70.00}
local heights = {0,   0}

local expResultsMosaic = { 116.250000000, 108.687500000}
local expResultsStrips = { 120.367187500, 112.632812500}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {37, 32}

for i = 1, #demTypes do

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Correct Values\n--------------------------------", demType))
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", catalog=contents, sort_by_index=true}))
    local tbl, err = dem:batchsample(lons, lats, heights)

    if err ~= 0 then
        print("FAILED to batchsample")
        runner.assert(false)
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
                        runner.assert(math.abs(el - expElevation) < sigma)
                    else
                        if not (el ~= el) then  -- not NaN
                            runner.assert(el > 100)  --All others should be > 100
                        end
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
            runner.assert(sampleCnt == expectedSamplesCnt)
        end
    end
end

-- Report Results --

runner.report()
