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

local demTypes = {"rema-mosaic", "rema-strips"}


local lons = {-170,  80}
local lats = { -85, -85}
local height = 0

local expResultsMosaic = {1341.6015625, 3408.6953125}
local expResultsStrips = {1349.5468750, 3409.8359375}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {6, 6}

--pipline is an output from projinfo tool ran as:
--projinfo -s EPSG:4326 -t EPSG:3031 -o proj
local pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +x_0=0 +y_0=0 +ellps=WGS84"
for i = 1, #demTypes do
    local isMosaic = (i == 1)

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Using proj pipeline\n--------------------------------", demType))
    print(string.format("%s", pipeline))
    print(string.format("\n----------------------------------------------------------------------------------------------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", proj_pipeline=pipeline}))

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
                        expElevation = expResultsMosaic[j]
                    else
                        expElevation = expResultsStrips[j]
                    end
                    runner.check(math.abs(el - expElevation) < sigma)
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

