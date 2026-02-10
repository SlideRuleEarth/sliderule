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
local height = 0

local expResultsMosaic = { 116.250000000, 108.687500000}
local expResultsStrips = { 120.367187500, 112.632812500}  -- Only first strip samples for each lon/lat strip group

local expSamplesMosaic = { 1,  1}
local expSamplesStrips = {37, 32}

-- Helper --

local function test(demType, expResults, expSamples)
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", catalog=contents, sort_by_index=true}))
    for j, lon in ipairs(lons) do
        local sampleCnt = 0
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",j, lon, lat), true)
        local el, fname
        for k, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %16.9fm   %s", k, lon, lat, el, fname))
            sampleCnt = sampleCnt + 1
            if k == 1 then -- Check all mosaics but only first strip for each POI
                runner.assert(math.abs(el - expResults[j]) < sigma)
            else
                if not (el ~= el) then  -- not NaN
                    runner.assert(el > 100)  --All others should be > 100
                end
            end
        end
        runner.assert(sampleCnt == expSamples[j])
    end
end

-- Self Test --

runner.unittest("ArcticDEM Mosaic Attributes", function()
    local dem = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=0, catalog=contents, sort_by_index=true}))
    local tbl, err = dem:sample(lons[1], lats[1], height) -- required in order to populate information for calls below
    local rows, cols = dem:dim()
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    local cellsize = dem:cell()
    runner.assert(rows == 3750100, tostring(rows))
    runner.assert(cols == 3700100, tostring(cols))
    runner.assert(lon_min == -4000100.0, tostring(lon_min))
    runner.assert(lat_min == -3400100.0, tostring(lat_min))
    runner.assert(lon_max ==  3400100.0, tostring(lon_max))
    runner.assert(lat_max ==  4100100.0, tostring(lat_max))
    runner.assert(cellsize == 2.0, tostring(cellsize))
end)

runner.unittest("ArcticDEM Strips Attributes", function()
    local dem = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, catalog=contents, sort_by_index=true}))
    local tbl, err = dem:sample(lons[1], lats[1], height) -- required in order to populate information for calls below
    local rows, cols = dem:dim()
    local cellsize = dem:cell()
    runner.assert(rows == 512)
    runner.assert(cols == 512)
    runner.assert(cellsize == 0.0)
end, {"long"})

runner.unittest("ArcticDEM Mosaic Correct Values", function()
    test("arcticdem-mosaic", expResultsMosaic, expSamplesMosaic)
end)

runner.unittest("ArcticDEM Strips Correct Values", function()
    test("arcticdem-strips", expResultsStrips, expSamplesStrips)
end, {"long"})

-- Report Results --

runner.report()
