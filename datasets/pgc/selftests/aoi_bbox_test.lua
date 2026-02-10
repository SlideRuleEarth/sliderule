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

local lons = {-150}
local lats = {  70}
local height = 0

local extentbox = {-151, 69, -149, 71}

local expResultsMosaic = {116.250000000000}
local expResultsStrips = {120.367187500000}  -- Only first strip samples for each lon/lat strip group
local expSamplesMosaic = {1}
local expSamplesStrips = {37}

-- Helper --

local function test(demType, expResults, expSamples)
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", aoi_bbox=extentbox, catalog=contents, sort_by_index=true}))
    for j, lon in ipairs(lons) do
        local sampleCnt = 0
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",j, lon, lat), true)
        local el, fname
        for k, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %16.12fm   %s", k, lon, lat, el, fname))
            sampleCnt = sampleCnt + 1
            if k == 1 then -- Check all mosaics but only first strip for each POI
                runner.assert(math.abs(el - expResults[j]) < sigma)
            else
                runner.assert(el > 100)  --All others
            end
        end
        runner.assert(sampleCnt == expSamples[j])
    end
end

-- Self Tests --

runner.unittest("ArcticDEM Mosaic Correct Values (AOI)", function()
    test("arcticdem-mosaic", expResultsMosaic, expSamplesMosaic)
end)

runner.unittest("ArcticDEM Strips Correct Values (AOI)", function()
    test("arcticdem-strips", expResultsStrips, expSamplesStrips)
end, {"long"})

-- Report Results --

runner.report()
