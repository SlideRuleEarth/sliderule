local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local lon = -178.0   -- DO NOT CHANGE lon and lat, later tests are hardcoded to these values!!!
local lat =   51.7
local height = 0

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

for i = 1, 2 do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0}))
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s sample\n--------------------------------", demType))
    local tbl, status = dem:sample(lon, lat, height)
    runner.check(status == true)
    runner.check(tbl ~= nil)

    local sampleCnt = 0

    for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %8.2fm  %s", j, el, fname))
        runner.check(el ~= -1000000)  --INVALID_SAMPLE_VALUE from VrtRaster.h
        runner.check(string.len(fname) > 0)
        sampleCnt = sampleCnt + 1
    end

    if demType == "arcticdem-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 14)
    end

    print(string.format("\n--------------------------------\nTest: %s dim\n--------------------------------", demType))
    local rows, cols = dem:dim()
    print(string.format("dimensions: rows: %d, cols: %d", rows, cols))
    -- runner.check(rows ~= 0)
    -- runner.check(cols ~= 0)

    print(string.format("\n--------------------------------\nTest: %s bbox\n--------------------------------", demType))
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print(string.format("lon_min: %d, lat_min: %d, lon_max: %d, lan_max: %d", lon_min, lat_min, lon_max, lat_max))
    -- runner.check(lon_min ~= 0)
    -- runner.check(lat_min ~= 0)
    -- runner.check(lon_max ~= 0)
    -- runner.check(lon_max ~= 0)

    print(string.format("\n--------------------------------\nTest: %s cellsize\n--------------------------------", demType))
    local cellsize = dem:cell()
    print(string.format("cellsize: %d", cellsize))

    if demType == "arcticdem-mosaic" then
        -- runner.check(cellsize == 2.0)
    else
        runner.check(cellsize == 0.0)
    end
end

for i = 1, 2 do

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
            print(string.format("(%02d) %6.2fm   cnt: %03d   qmask: 0x%x   min: %6.2f   max: %6.2f   mean: %6.2f   median: %6.2f   stdev: %6.2f   mad: %6.2f", j, el, cnt, flags, min, max, mean, median, stdev, mad))
            runner.check(el ~= 0.0)
            runner.check(min <= el)
            runner.check(max >= el)
            runner.check(mean ~= 0.0)
            runner.check(stdev ~= 0.0)
        end
        sampleCnt = sampleCnt + 1
    end

    if demType == "arcticdem-mosaic" then
        runner.check(sampleCnt == 1)
    else
        runner.check(sampleCnt == 14)
    end
end


local samplingRadius = 20
local demType = demTypes[2];
local url = "SETSM_s2s041_WV01_20181210_102001007A560E00_10200100802C2300"

print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true, with_flags=true, substr = url}))
runner.check(dem ~= nil)
local tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

local sampleCnt = 0

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local cnt = v["count"]
    print(string.format("(%02d) value: %6.8f   cnt: %03d   qmask: 0x%x %s", i, el, cnt, flags, fname))
    runner.check(el ~= -1000000) --INVALID_SAMPLE_VALUE from VrtRaster.h
    runner.check(string.len(fname) > 0)
    sampleCnt = sampleCnt + 1

    --Only one strip should be sampled with the url filter
    runner.check(flags == 4)    -- Qualit mask for this strip/sample, 4 means cloud
    runner.check(cnt == 317)    -- Valid samples used in calculating zonal stats
    runner.check(el > 452.4843 and el < 452.4850 )
end

runner.check(sampleCnt == 1)  -- Only one sample/strip returned



samplingRadius = 20
demType = demTypes[2];
local t0str = "2014:2:25:23:00:00"
local t1str = "2016:6:2:24:00:00"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter: t0=%s, t1=%s\n--------------------------------", demType, t0str, t1str))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, t0=t0str, t1=t1str}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local cnt = v["count"]
    print(string.format("(%02d) %6.2fm   cnt: %03d   qmask: 0x%x %s", i, el, cnt, flags, fname))
    runner.check(el ~= -1000000) --INVALID_SAMPLE_VALUE from VrtRaster.h
    runner.check(string.len(fname) > 0)
    sampleCnt = sampleCnt + 1

    if i == 1 then
        runner.check(flags == 4) -- Qualit mask for this strip/sample, 4 means cloud
        runner.check(cnt == 317) -- Valid samples used in calculating zonal stats
        runner.check(el > 773.03 and el < 773.04)
    else
        runner.check(flags == 0) -- Qualit mask for this strip/sample, 4 means cloud
        runner.check(cnt == 317) -- Valid samples used in calculating zonal stats
        runner.check(el > 80.2264 and el < 80.2266)
    end
end
runner.check(sampleCnt == 2)


t0str = "2021:2:3:1:0:0"
print(string.format("\n--------------------------------\nTest: %s Temporal Filter: t0=%s\n--------------------------------", demType, t0str))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, t0=t0str }))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local cnt = v["count"]
    print(string.format("(%02d) %6.2fm   cnt: %03d   qmask: 0x%x %s", i, el, cnt, flags, fname))
    runner.check(el ~= -1000000) --INVALID_SAMPLE_VALUE from VrtRaster.h
    runner.check(string.len(fname) > 0)
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 4)


t1str = "2021:2:3:1:0:0"
print(string.format("\n--------------------------------\nTest: %s Temporal Filter: t1=%s\n--------------------------------", demType, t1str))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, t1=t1str}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local cnt = v["count"]
    print(string.format("(%02d) %6.2fm   cnt: %03d   qmask: 0x%x %s", i, el, cnt, flags, fname))
    runner.check(el ~= -1000000) --INVALID_SAMPLE_VALUE from VrtRaster.h
    runner.check(string.len(fname) > 0)
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 10)

local tstr = "2021:2:4:23:3:0"
local expectedFile = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV03_20210204_10400100656B9F00_1040010065903500_2m_lsf_seg1_dem.tif"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter: closest_time=%s\n--------------------------------", demType, tstr))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, closest_time=tstr}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local fname = v["file"]
    print(string.format("(%02d) %s", i, fname))
    runner.check( expectedFile == fname )
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 1)


local tstr         = "2021:2:4:23:3:0"
local tstrOverride = "2016:6:0:0:0:0"
local expectedFile = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20160602_1030010057849C00_103001005607CA00_2m_lsf_seg1_dem.tif"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter Override closest_time: %s with %s\n--------------------------------", demType, tstr, tstrOverride))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, closest_time=tstr}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height, tstrOverride)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local fname = v["file"]
    print(string.format("(%02d) %s", i, fname))
    runner.check( expectedFile == fname )
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 1)


tstr = "2016:6:0:0:0:0"
expectedFile = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20160602_1030010057849C00_103001005607CA00_2m_lsf_seg1_dem.tif"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter: closest_time=%s\n--------------------------------", demType, tstr))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, closest_time=tstr}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local fname = v["file"]
    print(string.format("(%02d) %s", i, fname))
    runner.check( expectedFile == fname )
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 1)


tstr  = "2016:6:0:0:0:0"
t0str = "2021:2:3:1:0:0"
expectedFile = "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV03_20210204_10400100656B9F00_1040010065903500_2m_lsf_seg1_dem.tif"
print(string.format("\n--------------------------------\nTest: %s Temporal Filter: t0=%s, closest_time=%s\n--------------------------------", demType, t0str, tstr))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius,zonal_stats = true, with_flags = true, t0=t0str, closest_time=tstr}))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local fname = v["file"]
    print(string.format("(%02d) %s", i, fname))
    runner.check( expectedFile == fname )
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 1)


print(string.format("\n--------------------------------\nTest: %s fileId\n--------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius, with_flags = true}))
runner.check(dem ~= nil)

tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)
sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local time = v["time"]
    local fileid = v["fileid"]
    print(string.format("(%02d) %6.2fm   time: %.2f   qmask: 0x%x fileId: %02d  %s", i, el, time, flags, fileid, fname))
    runner.check(fileid == sampleCnt)  -- getting back 14 unique strips, each with fileid 0 to 13
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 14)
print("\n");

tbl, status = dem:sample(lon+1.0, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)
sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local time = v["time"]
    local fileid = v["fileid"]
    print(string.format("(%02d) %6.2fm   time: %.2f   qmask: 0x%x fileId: %02d  %s", i, el, time, flags, fileid, fname))
    runner.check(fileid == sampleCnt+14)  -- getting back 7 different strips, not previusly found, fileid 14 to 20
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 7)
print("\n");

tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)
sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    local time = v["time"]
    local fileid = v["fileid"]
    print(string.format("(%02d) %6.2fm   time: %.2f   qmask: 0x%x fileId: %02d  %s", i, el, time, flags, fileid, fname))
    runner.check(fileid == sampleCnt)  -- getting back the same 14 raster (from first sample call) fileid 0 to 13
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 14)


-- Correct values test for different POIs

local lons = {-178.0, -40, 100,   150}
local lats = {  51.7,  70,  70,    75}
height = 0

--NOTE: first mosaic result is sometimes 80.7063446
--                         and sometimes 80.7135010
--Figure out what is going on when reading this POI in VRT...

--local expResultsMosaic = { 80.7063446,  2969.2360839, 475.9624633, 19.8128376}
local expResultsMosaic = { 80.7135010,  2969.2360839, 475.9624633, 19.8128376}
local expResultsStrips = {452.4843750,  2968.0156250, 474.1562500, 10.2968750}  -- Only first strip samples for each lon/lat strip group

local expSamplesCnt = {14, 8, 4, 14}

for i = 1, 1 do
    local isMosaic = (i == 1)

    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Reading Correct Values\n--------------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

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
                        expected_value = expResultsMosaic[j]
                    else
                        expected_value = expResultsStrips[j]
                    end
                    -- print(string.format("(%02d) value: %16.7f  exp: %16.7f", k, el, expected_value))
                    expected_max = expected_value + 0.0000001
                    expected_min = expected_value - 0.0000001
                    runner.check(el <= expected_max and el >= expected_min)
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

