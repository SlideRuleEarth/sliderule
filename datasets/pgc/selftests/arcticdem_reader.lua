local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Self Test --

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

-- Correct values test for different POIs

local sigma = 1.0e-9

local lon = -150.0
local lat =   70.0
local height = 0


for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, sort_by_index=true}))
    runner.assert(dem ~= nil)
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %8.2f %s", i, el, fname))
        sampleCnt = sampleCnt + 1
    end

    print(string.format("\n--------------------------------\nTest: %s dim\n--------------------------------", demType))
    local rows, cols = dem:dim()
    print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
    runner.assert(rows ~= 0)
    runner.assert(cols ~= 0)

    print(string.format("\n--------------------------------\nTest: %s bbox\n--------------------------------", demType))
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print(string.format("lon_min: %.2f, lat_min: %.2f, lon_max: %.2f, lat_max: %.2f", lon_min, lat_min, lon_max, lat_max))

    print(string.format("\n--------------------------------\nTest: %s cellsize\n--------------------------------", demType))
    local cellsize = dem:cell()
    print(string.format("cellsize: %d", cellsize))

    if demType == "arcticdem-mosaic" then
        runner.assert(rows == 3750100)
        runner.assert(cols == 3700100)
        runner.assert(lon_min == -4000100.0)
        runner.assert(lat_min == -3400100.0)
        runner.assert(lon_max ==  3400100.0)
        runner.assert(lat_max ==  4100100.0)
        runner.assert(cellsize == 2.0)
    else
        runner.assert(rows == 512)
        runner.assert(cols == 512)
        runner.assert(math.abs(lon_min - -150.211795121361) < sigma)
        runner.assert(math.abs(lat_min -   69.558963399316) < sigma)
        runner.assert(math.abs(lon_max - -148.805309306014) < sigma)
        runner.assert(math.abs(lat_max -   70.772905719694) < sigma)
        runner.assert(cellsize == 0.0)
    end
end



local lons = {-150, -40, 100,   150}
local lats = {  70,  70,  70,    75}
height = 0

local expResultsMosaic = { 116.250000000,  2968.085937500, 475.742187500, 19.890625000}
local expResultsStrips = { 119.554687500,  2968.015625000, 474.156250000, 10.296875000}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {8, 8, 4, 14}

for i = 1, #demTypes do

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Correct Values\n--------------------------------", demType))
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", sort_by_index=true}))

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
                print(string.format("(%02d)   (%6.1f, %5.1f) %16.9fm   %s", k, lon, lat, el, fname))
                sampleCnt = sampleCnt + 1

                if k == 1 then -- Check all mosaics but only first strip for each POI
                    if demType == "arcticdem-mosaic" then
                        expElevation = expResultsMosaic[j]
                    else
                        expElevation = expResultsStrips[j]
                    end
                    runner.assert(math.abs(el - expElevation) < sigma)
                else
                    runner.assert(el > 10)  --All others
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
