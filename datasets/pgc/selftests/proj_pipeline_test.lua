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


local lons = {-150}
local lats = {  70}
local height = 0

local expResultsMosaic = { 116.2734375}
local expResultsStrips = { 120.5000000}  -- Only first strip samples for each lon/lat strip group
local expSamplesMosaic = {1}
local expSamplesStrips = {37}

local sigma = 1.0e-9

-- pipline is an output from projinfo tool ran as:
-- projinfo -s EPSG:4326 -t EPSG:3413 -o proj
--               "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +x_0=0 +y_0=0 +ellps=WGS84"
--
-- I added to it: (+x_0=10), the sampling returns different results for the same point because of the x_0 offset and the different projections
local pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +x_0=10 +y_0=0 +ellps=WGS84"

-- Helper --

local function test(demType, expResults, expSamples)
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", proj_pipeline=pipeline, catalog=contents, sort_by_index=true}))
    for j, lon in ipairs(lons) do
        local sampleCnt = 0
        local lat = lats[j]
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",j, lon, lat), true)
        local el, fname
        for k, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
            print(string.format("(%02d)   (%6.1f, %5.1f) %16.7fm   %s", k, lon, lat, el, fname))
            sampleCnt = sampleCnt + 1

            if k == 1 then -- Check all mosaics but only first strip for each POI
                runner.assert(math.abs(el - expResults[j]) < sigma)
            else
                if not (el ~= el) then  -- not NaN
                    runner.assert(el > 100)  --All others should be > 100
                end
            end
        end
        runner.assert(sampleCnt == expSamples)
    end
end

-- Self Test --

runner.unittest("ArcticDEM Mosaic PROJ Pipeline", function()
    test("arcticdem-mosaic", expResultsMosaic, expSamplesMosaic)
end)

runner.unittest("ArcticDEM Strips PROJ Pipeline", function()
    test("arcticdem-strips", expResultsStrips, expSamplesStrips)
end)

-- Report Results --

runner.report()
