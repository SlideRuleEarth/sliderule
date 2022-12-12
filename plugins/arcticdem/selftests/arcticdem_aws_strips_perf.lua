local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local asset_name = "arcticdem-local"
local arcticdem_local = core.getbyname(asset_name)

local failedSamples = 0
local verbose = false

local  lon = -178.0
local  lat =   51.7
local _lon = lon
local _lat = lat

print('\n------------------\nTest: AWS strips\n------------')
local dem = arcticdem.raster("strip", "NearestNeighbour", 0)
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat)
local stoptime = time.latch();
local dtime = stoptime - starttime

if status ~= true then
    failedSamples = failedSamples + 1
    print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
else
     for j, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %20f     %s", j, el, fname))
     end
end

print(string.format("ExecTime: %f, failed reads: %d", dtime, failedSamples))

local el, status
local maxPoints = 10000
failedSamples = 0

--[[
--]]

print(string.format("\n------------------\nTest: Reading %d times the same points\n------------------\n", maxPoints))

starttime = time.latch();
intervaltime = starttime


for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat)
    if status ~= true then
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
end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("\n%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))

-- os.exit()

failedSamples = 0
maxPoints = 1000
print(string.format("\n------------------\nTest: Reading %d different points in the same raster\n------------------\n", maxPoints))
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime

for i = 1, maxPoints
do
    tbl, status = dem:sample(lon, lat)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",i, lon, lat))
    else
        if verbose then
            for i, v in ipairs(tbl) do
                local el = v["value"]
                local fname = v["file"]
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

    modulovalue = 100
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print(string.format("Point: %d, (%.3f, %.3f), time: %.3f", i, lon, lat, dtime))
        intervaltime = time.latch();
    end

end

stoptime = time.latch();
dtime = stoptime-starttime
print(string.format("\n%d points read, time: %f, failed reads: %d", maxPoints, dtime, failedSamples))

os.exit()

-- Clean Up --
-- Report Results --

runner.report()

