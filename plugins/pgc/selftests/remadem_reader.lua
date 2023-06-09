local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local lon = -80.0   -- DO NOT CHANGE lon and lat, later tests are hardcoded to these values!!!
local lat = -80.0
local height = 0

local demTypes = {"rema-mosaic", "rema-strips"}
local demTypeCnt = 2

for i = 1, demTypeCnt do

    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0}))
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s sample\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat, height)
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

    if demType == "rema-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 10)
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

    if demType == "rema-mosaic" then
        runner.check(cellsize == 2.0)
    else
        runner.check(cellsize == 0.0)
    end

end

for i = 1, demTypeCnt do

    local demType = demTypes[i];
    local samplingRadius = 30
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true, with_flags=true}))

    runner.check(dem ~= nil)

    print(string.format("\n--------------------------------\nTest: %s Zonal Stats with qmask\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat, height)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local sampleCnt = 0

    local el, cnt, min, max, mean, median, stdev, mad, flags
    for j, v in ipairs(tbl) do
        el = v["value"]
        cnt = v["count"]
        min = v["min"]
        max = v["max"]
        mean = v["mean"]
        median = v["median"]
        stdev = v["stdev"]
        mad = v["mad"]
        flags = v["flags"]

        if el ~= -9999.0 then
            print(string.format("(%02d) value: %6.2f   cnt: %03d   qmask: 0x%x   min: %6.2f   max: %6.2f   mean: %6.2f   median: %6.2f   stdev: %6.2f   mad: %6.2f", j, el, cnt, flags, min, max, mean, median, stdev, mad))
            runner.check(el ~= 0.0)
            runner.check(min <= el)
            runner.check(max >= el)
            runner.check(mean ~= 0.0)
            runner.check(stdev ~= 0.0)
        end
        sampleCnt = sampleCnt + 1
    end

    if demType == "rema-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 10)
    end
end


demType = demTypes[1];
print(string.format("\n--------------------------------\nTest: %s Reading Correct Values\n--------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
sampleCnt = 0
local el, fname
for i, v in ipairs(tbl) do
    el = v["value"]
    fname = v["file"]
    print(string.format("(%02d) value: %6.2f   %s", i, el, fname))
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 1)

-- Compare sample value received from sample call to value read with GDAL command line utility.
-- To read the same value execute command below from a terminal (GDAL must be installed on the system)
-- gdallocationinfo -l_srs EPSG:7912 /vsis3/pgc-opendata-dems/rema/mosaics/v2.0/2m/2m_dem_tiles.vrt -80 -80

local expected_val = 328.015625 -- read using gdallocationinfo
local expected_max = expected_val + 0.000000001
local expected_min = expected_val - 0.000000001

runner.check(el <= expected_max and el >= expected_min)


demType = demTypes[2];
print(string.format("\n--------------------------------\nTest: %s Reading Correct Values\n--------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
sampleCnt = 0
local el, fname
for i, v in ipairs(tbl) do
    el = v["value"]
    fname = v["file"]
    print(string.format("(%02d)  value: %6.2f   %s", i, el, fname))
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 10)

-- Compare sample value received from sample call to value read with GDAL command line utility.
-- To read the same value execute command below from a terminal (GDAL must be installed on the system)
-- gdallocationinfo -l_srs EPSG:7912 /vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV03_20201105_1040010062706C00_104001006337FF00_2m_lsf_seg1_dem.tif -80 -80

expected_val =  333.6171875 -- read using gdallocationinfo
expected_max = expected_val + 0.000000001
expected_min = expected_val - 0.000000001

runner.check(el <= expected_max and el >= expected_min)

-- Report Results --

runner.report()

