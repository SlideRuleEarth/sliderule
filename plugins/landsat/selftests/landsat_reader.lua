local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")
local td = runner.rootdir(arg[0])

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)

local geojsonfile = td.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  lon = -179.0
local  lat = 51.0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI)\n-------------------------------------------------"))

local demType = "landsat-hls"


local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog = contents }))
local sampleCnt = 0
local ndvi = 0
local tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]

        sampleCnt = sampleCnt + 1
        if sampleCnt == 1 then
            ndvi = value
        end
        print(string.format("(%02d) value %10.3f, fname: %s", j, value, fname))

        -- only group names with algo string should be returned as a file name, example:
        -- HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDVI"}
        runner.check(string.find(fname, "{\"algo\": \"NDVI\"}"))
    end
end

runner.check(sampleCnt == 9)

local expected_value = -0.259439 -- first ndvi from the group of samples
local expected_max = expected_value + 0.00001
local expected_min = expected_value - 0.00001

runner.check(ndvi <= expected_max and ndvi >= expected_min)
dem = nil


print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI, NDSI, NDWI)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents }))
sampleCnt = 0
local ndviCnt= 0
local ndsiCnt= 0
local ndwiCnt= 0
tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]

        sampleCnt = sampleCnt + 1

        print(string.format("(%02d) value %10.3f, fname: %s", j, value, fname))

        if string.find(fname, "NDSI") then
            ndsiCnt = ndsiCnt + 1
            if sampleCnt == 1 then
                local ndsi = value
                expected_value = 0.06517 -- first ndwi from the group of samples
                expected_max = expected_value + 0.00001
                expected_min = expected_value - 0.00001
                runner.check(ndsi <= expected_max and ndsi >= expected_min)
            end
        end

        if string.find(fname, "NDWI") then
            ndwiCnt = ndwiCnt + 1
            if sampleCnt == 1 then
                local ndwi = value
                expected_value = -0.24315 -- first ndwi from the group of samples
                expected_max = expected_value + 0.00001
                expected_min = expected_value - 0.00001
                runner.check(ndwi <= expected_max and ndwi >= expected_min)
            end
        end

        if string.find(fname, "NDVI") then
            ndviCnt = ndviCnt + 1
            if sampleCnt == 1 then
                local ndvi = value
                expected_value = -0.25944 -- first ndvi from the group of samples
                expected_max = expected_value + 0.00001
                expected_min = expected_value - 0.00001
                runner.check(ndvi <= expected_max and ndvi >= expected_min)
            end
        end
    end
end

runner.check(sampleCnt == 27)
runner.check(ndviCnt == 9)
runner.check(ndsiCnt == 9)
runner.check(ndwiCnt == 9)

local url = "HLS.S30.T01UCS.2021001T22594"

print(string.format("\n--------------------------------\nTest: %s URL/GROUP Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, substr = url, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents }))
sampleCnt = 0
local invalidSamples = 0

tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]

        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %10.3f, fname: %s", j, value, fname))
        if string.find(fname, url) == false then
            invalidSamples = invalidSamples + 1
        end

    end
end

runner.check(sampleCnt == 3)
runner.check(invalidSamples == 0)


local tstr = "2021:2:4:23:3:0"
local expectedGroup = "T01UCS.2021026T225819.v2.0"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter: closest_time=%s\n--------------------------------", demType, tstr))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true,  closest_time=tstr, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents }))
runner.check(dem ~= nil)
tbl, status = dem:sample(lon, lat)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
local value, fname
for i, v in ipairs(tbl) do
    fname = v["file"]
    value = v["value"]
    print(string.format("(%02d) value %10.3f, fname: %s", i, value, fname))
    runner.check( string.find(fname, expectedGroup))
    sampleCnt = sampleCnt + 1
end
runner.check(sampleCnt == 3)  -- 3 groups with closest time


print(string.format("\n-------------------------------------------------\nLandsat Plugin test (BO3 and qualit flags)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"B03"}, catalog = contents }))
local fmaskCnt  = 0
local sampleCnt = 0
local b03 = 0
tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, flags
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        flags = v["flags"]

        print(string.format("(%02d) value: %10.3f qmask: 0x%x, %s", j, value, flags, fname))

        if string.find(fname, "B03.tif") then
            sampleCnt = sampleCnt + 1
            if sampleCnt == 1 then
                b03 = value
                runner.check(flags == 0xC2) -- first qualit mask
            end
        end

        if string.find(fname, "Fmask.tif") then
            fmaskCnt = fmaskCnt + 1
        end

        runner.check(flags ~= 0) -- all flags should be non zero
    end
end

runner.check(sampleCnt == 9)
runner.check(fmaskCnt  == 0)

expected_value = 523.0 -- first b03 from the group of samples
expected_max = expected_value + 0.00001
expected_min = expected_value - 0.00001
runner.check(b03 <= expected_max and b03 >= expected_min)


print(string.format("\n-------------------------------------------------\nLandsat Plugin test (BO3 and Fmask raster)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"Fmask","B03"}, catalog = contents }))
fmaskCnt  = 0
sampleCnt = 0
b03 = 0
tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, flags
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        flags = v["flags"]

        print(string.format("(%02d) value: %10.3f qmask: 0x%x, %s", j, value, flags, fname))

        if string.find(fname, "B03.tif") then
            sampleCnt = sampleCnt + 1
            if sampleCnt == 1 then
                b03 = value
                runner.check(flags == 0xC2) -- first qualit mask
            end
        end

        if string.find(fname, "Fmask.tif") then
            fmaskCnt = fmaskCnt + 1
        end

        runner.check(flags ~= 0) -- all flags should be non zero
    end
end

runner.check(sampleCnt == 9)
runner.check(fmaskCnt  == 9)

expected_value = 523.0 -- first b03 from the group of samples
expected_max = expected_value + 0.00001
expected_min = expected_value - 0.00001
runner.check(b03 <= expected_max and b03 >= expected_min)



print(string.format("\n-------------------------------------------------\nLandsat Plugin test (BO3 Zonal stats)\n-------------------------------------------------"))
local samplingRadius = 120
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = samplingRadius, zonal_stats=true, with_flags=true, bands = {"B03"}, catalog = contents }))
sampleCnt = 0
b03 = 0
tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, cnt, min, max, mean, median, stdev, mad, flags
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        cnt = v["count"]
        min = v["min"]
        max = v["max"]
        mean = v["mean"]
        median = v["median"]
        stdev = v["stdev"]
        mad = v["mad"]
        flags = v["flags"]

        if el ~= -9999.0 then
            print(string.format("(%02d) value: %10.2f   cnt: %03d   qmask: 0x%x   min: %10.2f   max: %10.2f   mean: %10.2f   median: %10.2f   stdev: %10.2f   mad: %10.2f", j, value, cnt, flags, min, max, mean, median, stdev, mad))
            runner.check(value ~= 0.0)
            runner.check(min <= value)
            runner.check(max >= value)
            runner.check(mean ~= 0.0)
            runner.check(stdev ~= 0.0)
        end

        sampleCnt = sampleCnt + 1
        if sampleCnt == 1 then
            b03 = value
        end

        runner.check(string.find(fname, "B03.tif"))
    end
end
runner.check(sampleCnt == 9)
expected_value = 523.0 -- first b03 from the group of samples
expected_max = expected_value + 0.00001
expected_min = expected_value - 0.00001
runner.check(b03 <= expected_max and b03 >= expected_min)



print(string.format("\n-------------------------------------------------\nMany Rasters (189) Test\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0,
                            bands = {"VAA", "VZA", "Fmask","SAA", "SZA", "NDSI", "NDVI", "NDWI",
                                     "B01", "B02", "B03", "B04", "B05", "B06",
                                     "B07", "B08", "B09", "B10", "B11", "B12", "B8A", },
                            catalog = contents }))
sampleCnt = 0
tbl, status = dem:sample(lon, lat)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        sampleCnt = sampleCnt + 1
        -- print(string.format("(%02d) value %10.3f, fname: %s", j, value, fname))
    end
end

runner.check(sampleCnt == 189)

-- Grand Mesa test has 26183 samples/ndvi Results
-- Limit the number of samples for selftest.
local maxSamples = 100
local linesRead = 0

local geojsonfile = td.."../data/grand_mesa.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

local poifile = td.."../data/grand_mesa_poi.txt"
local f = io.open(poifile, "r")
-- read in array of POI from file
local arr = {}
for l in f:lines() do
  local row = {}
  for snum in l:gmatch("(%S+)") do
    table.insert(row, tonumber(snum))
  end
  table.insert(arr, row)
  linesRead = linesRead + 1
  if linesRead > maxSamples then
    break
  end
end
f:close()

local ndvifile = td.."../data/grand_mesa_ndvi.txt"
local f = io.open(ndvifile, "r")
-- read in array of NDVI values
local ndvi_results = {}
linesRead = 0

for l in f:lines() do
  table.insert(ndvi_results, tonumber(l))
  linesRead = linesRead + 1
  if linesRead > maxSamples then
    break
  end
end
f:close()

local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"
local badFileCnt = 0
local badNDVICnt = 0

print(string.format("\n-------------------------------------------------\nLandsat Grand Mesa test\n-------------------------------------------------"))

local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, closest_time = "2022-01-05T00:00:00Z", bands = {"NDVI"}, catalog = contents }))
sampleCnt = 0

local starttime = time.latch();

for i=1, maxSamples do
    local  lon = arr[i][1]
    local  lat = arr[i][2]
    local  tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read", lon, lat))
    else
        local ndvi, fname
        for j, v in ipairs(tbl) do
            ndvi = v["value"]
            fname = v["file"]

            runner.check(fname == expectedFile)
            if fname ~= expectedFile then
                print(string.format("======> wrong group: %s", fname))
                badFileCnt = badFileCnt + 1
            end

            local expectedNDVI = ndvi_results[i]
            local delta = 0.0000000000000001
            local min = expectedNDVI - delta
            local max = expectedNDVI + delta

            runner.check(ndvi <= max and ndvi >= min)
            if (ndvi > max or ndvi < min) then
                print(string.format("======> wrong ndvi: %0.16f, expected: %0.16f", ndvi, expectedNDVI))
                badNDVICnt = badNDVICnt + 1
            end
        end
    end
    sampleCnt = sampleCnt + 1
end

local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("Samples: %d, ExecTime: %f", sampleCnt, dtime))
runner.check(sampleCnt == maxSamples)
runner.check(badFileCnt == 0)
runner.check(badNDVICnt == 0)

-- Report Results --

runner.report()

