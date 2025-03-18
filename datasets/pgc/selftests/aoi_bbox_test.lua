local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Self Test --

local sigma = 1.0e-9

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

--One sample run with extent limit
local lons = {-150}
local lats = {  70}
local height = 0

local extentbox = {-151, 69, -149, 71}

local expResultsMosaic = {116.250000000000}
local expResultsStrips = {119.554687500000}  -- Only first strip samples for each lon/lat strip group
local expSamplesCnt = {8}

for i = 1, #demTypes do
    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Reading Correct Values with AOI box\n--------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", aoi_bbox=extentbox, sort_by_index=true}))

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
                print(string.format("(%02d)   (%6.1f, %5.1f) %16.12fm   %s", k, lon, lat, el, fname))
                sampleCnt = sampleCnt + 1

                if k == 1 then -- Check all mosaics but only first strip for each POI
                    if demType == "arcticdem-mosaic" then
                        expElevation = expResultsMosaic[j]
                    else
                        expElevation = expResultsStrips[j]
                    end
                    runner.assert(math.abs(el - expElevation) < sigma)
                else
                    runner.assert(el > 116)  --All others
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
        runner.assert(sampleCnt == expectedSamplesCnt)
    end
end

-- Report Results --

runner.report()
