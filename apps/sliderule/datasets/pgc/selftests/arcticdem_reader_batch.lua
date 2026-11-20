local runner = require("test_executive")
local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/arcticdem_strips.geojson"
local contents = readgeojson.load(jsonfile)

local sigma = 1.0e-9
local lons = {-150, -149.95}
local lats = {  70,   70.00}
local heights = {0,   0}

local expResultsMosaic = { 116.250000000, 108.687500000}
local expResultsStrips = { 120.367187500, 112.632812500}  -- Only first strip samples for each lon/lat strip group
local expSamplesMosaic = {1, 1}
local expSamplesStrips = {37, 32}

-- Helper --

local function test(demType, expResults, expSamples)
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", catalog=contents, sort_by_index=true}))
    local tbl, err = dem:batchsample(lons, lats, heights)
    runner.assert(err == 0, "FAILED to batchsample", true)
    for j, pointSamples in ipairs(tbl) do
        local lon, lat = lons[j], lats[j]
        local sampleCnt = #pointSamples
        assert(sampleCnt > 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read samples", j, lon, lat))
        for k, sample in ipairs(pointSamples) do
            local el = sample["value"]
            local fname = sample["file"]
            print(string.format("    Sample (%02d): %16.9fm   %s", k, el, fname))
            if k == 1 then -- Check all mosaics but only first strip for each POI
                runner.assert(math.abs(el - expResults[j]) < sigma)
            else
                if not (el ~= el) then  -- not NaN
                    runner.assert(el > 100)  --All others should be > 100
                end
            end
        end
        runner.assert(sampleCnt == expSamples[j], string.format("incorrect number of samples: %d", sampleCnt))
    end
end

-- Self Test --

runner.unittest("ArcticDEM Mosaic Correct Values (batch)", function()
    test("arcticdem-mosaic", expResultsMosaic, expSamplesMosaic)
end)

runner.unittest("ArcticDEM Strips Correct Values (batch)", function()
    test("arcticdem-strips", expResultsStrips, expSamplesStrips)
end, {"long"})

-- Report Results --

runner.report()
