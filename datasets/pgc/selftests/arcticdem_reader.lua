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
local jsonfile = td .. "../data/arcticdem_strips.json"
local contents = readgeojson.load(jsonfile)

-- Self Test --

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

-- Correct values test for different POIs

local sigma = 1.0e-9

local lon = -150.0
local lat =   70.0
local height = 0


for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, catalog=contents, sort_by_index=true}))
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
        runner.assert(cellsize == 0.0)
    end
end


local lons = {-150, -149.95}
local lats = {  70,   70.00}
height = 0

local expResultsMosaic = { 116.250000000, 108.687500000}
local expResultsStrips = { 120.367187500, 112.632812500}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {37, 32}

for i = 1, #demTypes do

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Correct Values\n--------------------------------", demType))
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", catalog=contents, sort_by_index=true}))

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
                    if not (el ~= el) then  -- not NaN
                        runner.assert(el > 100)  --All others should be > 100
                    end
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
