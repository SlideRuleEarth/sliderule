console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir(nil,true) -- looks for asset_directory.csv in same directory this script is located in

-- Setup --

local verbose = true       --Set to see elevation value and tif file name returned
local verboseErrors = false --Set to see which sample reads failed

local zonalStats = false
local qualityMask = true

local maxPoints     = 1000

local starttime = time.latch();
local intervaltime = starttime

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}
for d = 1, 2 do
    local  lonIncrement  = 0.5
    local  latIncrement  = 0.1
    local  failedSamples = 0
    local  lon = -100.50
    local  lat = 76.34
    -- local  lon = -65.50
    -- local  lat = 83.29
    local  demType = demTypes[d];
    local  dem     = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, zonal_stats=zonalStats, with_flags=qualityMask}))

    print(string.format("\n-------------------------------------------------------------\nTest: %s BITMASK/FLAGS test reading %d points\n-------------------------------------------------------------", demType, maxPoints))

    for i = 1, maxPoints
    do
        local tbl, status = dem:sample(lon, lat)
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
        lat = lat + latIncrement
    end

    stoptime = time.latch();
    dtime = stoptime-starttime
    print(string.format("%d points sampled, ExecTime: %f, failed raster reads: %d", maxPoints, dtime, failedSamples))
end

sys.quit()
