local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Setup --

local failedSamples = 0
-- local verbose = true
local verbose = false

local  lon = -180.00
local  lat = -80.0
local  height = 0
local _lon = lon
local _lat = lat

print('\n------------------\nTest: AWS mosaic\n------------')
local dem = geo.raster(geo.parms({asset="rema-mosaic", algorithm="NearestNeighbour", radius=0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
local dtime = stoptime - starttime

if err ~= 0 then
    failedSamples = failedSamples + 1
    print(string.format("Point: (%.3f, %.3f) ======> FAILED to read",lon, lat))
else
    if verbose then
        for j, v in ipairs(tbl) do
            local el = v["value"]
            local fname = v["file"]
            print(string.format("(%02d) %20f     %s", j, el, fname))
        end
    end
end

print(string.format("ExecTime: %f, failed reads: %d", dtime, failedSamples))

local max_cnt = 100000

--[[
--]]

print('\n------------------\nTest: Reading', max_cnt, '  points (THE SAME POINT)\n------------')
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
    end
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("\n%d points read, time: %f, failed reads: %d", max_cnt, dtime, failedSamples))


--[[
--]]

-- About 500 points read from the same raster before opening a new one
max_cnt = 1000
print('\n------------------\nTest: Reading', max_cnt, '  different points (Average case of DIFFERENT RASTER)\n------------')
lon = _lon
lat = _lat
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
    end

    modulovalue = 1000
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %d, (%.3f, %.3f), time: %.3f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end

    lon = lon + 0.001
end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(string.format("%d points read, time: %f, failed reads: %d", max_cnt, dtime, failedSamples))

-- os.exit();



-- Every point in different mosaic raster (absolute worse case performance)
-- Walking around circle hitting different raster each time
max_cnt = 120

print('\n------------------\nTest: Reading', max_cnt, '  different points (DiFERENT RASTERs, walk around circle)\n------------')
lon = _lon
lat = _lat
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
    else
        if verbose then
            for j, v in ipairs(tbl) do
                local el = v["value"]
                local fname = v["file"]
                print(string.format("(%02d) lon: %f, lat: %f, %20f     %s", j, lon, lat, el, fname))
            end
        end
    end

    modulovalue = 10
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %d, (%.3f, %.3f), time: %.3f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end

    -- NOTE: COGs are 1x1 degree but if I increment lon by 1.1 or 1.2
    --       ocasionally I hit the same raster twice.
    --  Incrementing by 1.5 degree gives absolute worse case scenario.
    --  Every point read is from different raster then the last one.
    lon = lon + 1.5
end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(string.format("%d points read, time: %f, failed reads: %d", max_cnt, dtime, failedSamples))

sys.quit()
