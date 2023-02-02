console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Setup --

-- local verbose = true
local verbose = true  --Set to see elevation value and tif file name returned
local verboseErrors = false --Set to see which sample reads failed

local  lon = -90.50
local  lat = 66.34
local _lon = lon
local _lat = lat

local zonalStats = false
local qualityMask = true

local tbl, status

local lonIncrement  = 1.0
local failedSamples = 0
local maxPoints     = 20

print(string.format("\nMOSAIC, Quality Bitmask Test - Walking Arctic Circle"))

--[[
--]]
starttime = time.latch();
intervaltime = starttime

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}
for d = 1, 2 do
    local demType = demTypes[d];
    local dem     = geo.raster(demType, "NearestNeighbour", 0, zonalStats, qualityMask)

    print(string.format("\n------------------------------------------\nTest: %s BITMASK/FLAGS test\n------------------------------------------", demType))

    for i = 1, maxPoints
    do
        tbl, status = dem:sample(lon, lat)
        if status ~= true then
            failedSamples = failedSamples + 1
            if verboseErrors then
                print(string.format("(POINT: %4d)  (%.2f, %.2f) ======> FAILED",i, lon, lat))
            end
        else
            if verbose then
                for j, v in ipairs(tbl) do
                    local el = v["value"]
                    local flags = v["flags"]
                    local fname = v["file"]
                    print(string.format("(POINT: %4d)  (RASTER: %2d) (%.2f, %.2f) %10.2f, qmask: 0x%x, %s",i, j, lon, lat, el, flags, fname))
                end
            else
                modulovalue = 10
                if (i % modulovalue == 0) then
                    midtime = time.latch();
                    dtime = midtime-intervaltime
                    for j, v in ipairs(tbl) do
                        local el = v["value"]
                        local flags = v["flags"]
                        print(string.format("(POINT: %4d)  (RASTER: %2d)  (%.2f, %.2f) value: %-8.2f qmask: 0x%x, time: %.2f", i, j, lon, lat, el, flags, dtime))
                        break
                    end
                    intervaltime = time.latch();
                end
            end
        end

        lon = lon + lonIncrement
    end

    stoptime = time.latch();
    dtime = stoptime-starttime
    print(string.format("%d points sampled, ExecTime: %f, failed: %d", maxPoints, dtime, failedSamples))
end

os.exit()
