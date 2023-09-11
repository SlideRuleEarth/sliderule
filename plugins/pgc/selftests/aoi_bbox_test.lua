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

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

--One sample run with extent limit
local lons = {-150}
local lats = {  70}
height = 0

local extentbox = {-151, 69, -149, 71}

local expResultsMosaic = {116.250000000000}
local expResultsStrips = {119.554687500000}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {8}

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
                print(string.format("(%02d)   (%6.1f, %5.1f) %16.12fm   %s", k, lon, lat, el, fname))
                sampleCnt = sampleCnt + 1

                if k == 1 then -- Check all mosaics but only first strip for each POI
                    if demType == "arcticdem-mosaic" then
                        expElevation = expResultsMosaic[j]
                    else
                        expElevation = expResultsStrips[j]
                    end
                    runner.check(math.abs(el - expElevation) < sigma)
                else
                    runner.check(el > 116)  --All others
                end
            end
        end

        if demType == "arcticdem-mosaic" then
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

