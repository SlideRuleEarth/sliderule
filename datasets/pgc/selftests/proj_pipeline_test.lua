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

local sigma = 1.0e-9

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

local lons = {-150}
local lats = {  70}
height = 0

local expResultsMosaic = { 116.2734375}
local expResultsStrips = { 120.5000000}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {37}

-- pipline is an output from projinfo tool ran as:
-- projinfo -s EPSG:4326 -t EPSG:3413 -o proj
--               "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +x_0=0 +y_0=0 +ellps=WGS84"
--
-- I added to it: (+x_0=10), the sampling returns different results for the same point because of the x_0 offset and the different projections
local pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +x_0=10 +y_0=0 +ellps=WGS84"

for i = 1, #demTypes do
    local isMosaic = (i == 1)

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Using proj pipeline\n--------------------------------", demType))
    print(string.format("%s", pipeline))
    print(string.format("\n----------------------------------------------------------------------------------------------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", proj_pipeline=pipeline, catalog=contents, sort_by_index=true}))
    -- dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", catalog=contents, sort_by_index=true}))

    for j, lon in ipairs(lons) do
        local sampleCnt = 0
        lat = lats[j]
        tbl, err = dem:sample(lon, lat, height)
        if err ~= 0 then
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
        end

        if isMosaic == true then
            expectedSamplesCnt = 1
        else
            expectedSamplesCnt = expSamplesCnt[j]
            print("\n")
        end
        -- print(string.format("(%02d) value: %d  exp: %d", i, sampleCnt, expectedSamplesCnt))
        runner.assert(sampleCnt == expectedSamplesCnt)
    end
end

-- Report Results --

runner.report()
