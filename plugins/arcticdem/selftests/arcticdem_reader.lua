local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local lon = -178.0
local lat =   51.7

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

for i = 1, 2 do

    local demType = demTypes[i];
    local dem = geo.raster(demType, "NearestNeighbour", 0)

    runner.check(dem ~= nil)

    print(string.format("\n--------------------------------\nTest: %s sample\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local sampleCnt = 0

    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %8.2f %s", i, el, fname))
        runner.check(el ~= -1000000)  --INVALID_SAMPLE_VALUE from VrtRaster.h
        runner.check(string.len(fname) > 0)
        sampleCnt = sampleCnt + 1
    end

    if demType == "arcticdem-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt >= 28)
    end

    print(string.format("\n--------------------------------\nTest: %s dim\n--------------------------------", demType))
    local rows, cols = dem:dim()
    print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
    runner.check(rows ~= 0)
    runner.check(cols ~= 0)

    print(string.format("\n--------------------------------\nTest: %s bbox\n--------------------------------", demType))
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print(string.format("lon_min: %d, lat_min: %d, lon_max: %d, lan_max: %d", lon_min, lat_min, lon_max, lat_max))
    runner.check(lon_min ~= 0)
    runner.check(lat_min ~= 0)
    runner.check(lon_max ~= 0)
    runner.check(lon_max ~= 0)

    print(string.format("\n--------------------------------\nTest: %s cellsize\n--------------------------------", demType))
    local cellsize = dem:cell()
    print(string.format("cellsize: %d", cellsize))

    if demType == "arcticdem-mosaic" then
        runner.check(cellsize == 2.0)
    else
        runner.check(cellsize == 0.0)
    end

end


for i = 1, 2 do

    local demType = demTypes[i];
    local samplingRadius = 30
    local dem = geo.raster(demType, "NearestNeighbour", samplingRadius, true)

    runner.check(dem ~= nil)

    print(string.format("\n--------------------------------\nTest: %s Zonal Stats\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local el, cnt, min, max, mean, median, stdev, mad
    for j, v in ipairs(tbl) do
        el = v["value"]
        cnt = v["count"]
        min = v["min"]
        max = v["max"]
        mean = v["mean"]
        median = v["median"]
        stdev = v["stdev"]
        mad = v["mad"]

        if el ~= -9999.0 then
            print(string.format("(%02d) value: %6.2f   cnt: %03d   min: %6.2f   max: %6.2f   mean: %6.2f   median: %6.2f   stdev: %6.2f   mad: %6.2f", j, el, cnt, min, max, mean, median, stdev, mad))
            runner.check(el ~= 0.0)
            runner.check(min <= el)
            runner.check(max >= el)
            runner.check(mean ~= 0.0)
            runner.check(stdev ~= 0.0)
        end
    end
end

-- Report Results --

runner.report()

