console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Setup --

local failedSamples = 0
-- local verbose = true
local verbose = false

local  lon = -180.00
local  lat = 66.34  -- Arctic Circle lat
local  height = 0
local _lon = lon
local _lat = lat

print(string.format("\n------------------------------------\nMosaics reading one point\n------------------------------------"))
local dem = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=0}))
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
local stoptime = time.latch();
local dtime = stoptime - starttime

if status ~= true then
    failedSamples = failedSamples + 1
    print(string.format("Point: %d, (%7.2, %.2) ======> FAILED to read",i, lon, lat))
else
    for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %20f     %s", j, el, fname))
    end
end
print(string.format("%d points read, time: %f, failed reads: %d", 1, dtime, failedSamples))


local maxPoints = 100000
print(string.format("\n------------------------------------\nMosaics reading %d same points\n------------------------------------", maxPoints))
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%7.2, %.2) ======> FAILED to read",i, lon, lat))
    end
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))

-- os.exit()

-- About 500 points read from the same raster before opening a new one
maxPoints = 100
print(string.format("\n------------------------------------\nMosaics reading %d different points\n------------------------------------", maxPoints))
lon = _lon
lat = _lat
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%7.2f, %.2f) ======> FAILED to read",i, lon, lat))
    end

    lon = lon + 0.001
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))


-- Every point in different mosaic raster (absolute worse case performance)
-- Walking around arctic circle hitting different raster each time
maxPoints = 100
print(string.format("\n------------------------------------\nMosaics reading %d different geocells\n------------------------------------", maxPoints))
lon = _lon
lat = _lat
failedSamples = 0
starttime = time.latch();
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%7.2, %.2) ======> FAILED to read",i, lon, lat))
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
        print(string.format("Point: %4d, (%7.2f, %.2f), time: %.2f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end

    lon = lon + 0.7
end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))

sys.quit()
