console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Setup --

local failedSamples = 0
local verbose = false

local  lon = -150.00
local  lat = 66.34  -- Arctic Circle lat
local  height = 0
local _lon = lon
local _lat = lat

print(string.format("\n------------------------------------\nStrips reading one point\n------------------------------------"))
local dem = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0}))
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
local stoptime = time.latch();
local dtime = stoptime - starttime

if status ~= true then
    failedSamples = failedSamples + 1
    print(string.format("Point (%7.2f, %.2f) ======> FAILED to read",lon, lat))
else
     for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %20f     %s", j, el, fname))
     end
end
print(string.format("%d points read, time: %f, failed reads: %d", 1, dtime, failedSamples))

local el, status
local maxPoints = 10000
failedSamples = 0
print(string.format("\n------------------------------------\nStrips reading %d same points\n------------------------------------", maxPoints))

starttime = time.latch();
intervaltime = starttime
for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %6d, (%7.2f, %.2f) ======> FAILED to read",i, lon, lat))
    end

    modulovalue = 1000
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %6d, (%7.2f, %.2f), time: %.2f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))


failedSamples = 0
maxPoints = 100
print(string.format("\n------------------------------------\nStrips reading %d different points\n------------------------------------", maxPoints))
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime


for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    sampledStripsCnt = 0
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %3d, (%7.2f, %.2f) ======> FAILED to read",i, lon, lat))
    else
        for i, v in ipairs(tbl) do
            local el = v["value"]
            local fname = v["file"]
            sampledStripsCnt = sampledStripsCnt + 1
            if verbose then
                print(string.format("(%02d) %20f     %s", i, el, fname))
            end
        end
    end

    if (i % 40 == 0) then
        lon = _lon
        lat = _lat
    else
        lon = lon + 0.01
        lat = lat + 0.01
    end

    modulovalue = 10
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %3d, (%7.2f, %.2f), time: %.2f,  strips: %d", i, lon, lat, dtime, sampledStripsCnt))
        intervaltime = time.latch();
    end
end
stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))


failedSamples = 0
maxPoints = 100
print(string.format("\n------------------------------------\nStrips reading %d different geocells\n------------------------------------", maxPoints))
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime


for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat, height)
    sampledStripsCnt = 0
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %3d, (%7.2f, %.2f) ======> FAILED to read",i, lon, lat))
    else
        for i, v in ipairs(tbl) do
            local el = v["value"]
            local fname = v["file"]
            sampledStripsCnt = sampledStripsCnt + 1
            if verbose then
                print(string.format("(%02d) %20f     %s", i, el, fname))
            end
        end
    end

    modulovalue = 1
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %3d, (%7.2f, %.2f), time: %.2f,  strips: %d", i, lon, lat, dtime, sampledStripsCnt))
        intervaltime = time.latch();
    end

    lon = lon + 1.5
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))

sys.quit()

