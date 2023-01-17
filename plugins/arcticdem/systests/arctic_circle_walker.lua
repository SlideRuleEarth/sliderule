console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Setup --

-- local verbose = true
local verbose = false        --Set to see elevation value and tif file name returned
local verboseErrors = true   --Set to see which sample reads failed

local  lon = 180.00
local  lat = 66.34  -- Arctic Circle lat
local _lon = lon
local _lat = lat

local dem = geo.raster("arcticdem-mosaic", "NearestNeighbour", 0)
local tbl, status

local lonIncrement  = 1.5
local failedSamples = 0
local maxPoints     = 130

print(string.format("\nMOSAIC, Walking Arctic Circle test"))

--[[
--]]
print(string.format("\n*** Worse Case, each sample in new rasters"));
print(string.format("*** Sampling %d points, incrementing lon by %.3f degrees, starting at lon: %.3f, lat: %.3f",maxPoints, lonIncrement, lon, lat))
-- Every point in different mosaic raster (absolute worse case performance)
-- Walking around arctic circle hitting different raster each time
-- NOTE: COGs are 1x1 degree but if I increment lon by 1.1 or 1.2
--       ocasionally I hit the same raster twice.
--  Incrementing by 1.5 degree gives absolute worse case scenario.
--  Every point read is from different raster then the last one.
starttime = time.latch();
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat)
    if status ~= true then
        failedSamples = failedSamples + 1
        if verboseErrors then
            print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
        end
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

    lon = lon + lonIncrement
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points sampled, ExecTime: %f, failed: %d", maxPoints, dtime, failedSamples))


lon = _lon
lat = _lat
lonIncrement  = 0.01
failedSamples = 0
maxPoints     = 1000
max = 1.0 / lonIncrement
min = 10

print(string.format("\n*** Average Case, %d to %d samples in one raster", min, max))
print(string.format("*** Sampling %d points, incrementing lon by %.3f degrees, starting at lon: %.3f, lat: %.3f",maxPoints, lonIncrement, lon, lat))
starttime = time.latch()
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat)
    if status ~= true then
        failedSamples = failedSamples + 1
        if verboseErrors then
            print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
        end
    end

    modulovalue = 100
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %d, (%.3f, %.3f), time: %.3f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end

    lon = lon + lonIncrement
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("%d points sampled, ExecTime: %f, failed: %d", maxPoints, dtime, failedSamples))

os.exit()
