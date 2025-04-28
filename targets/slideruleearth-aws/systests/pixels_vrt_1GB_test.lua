local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local loopcnt = 5


local demType = "rema-mosaic"
print(string.format("\n------------------------------\n%s\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

-- AOI in pixels
local ulx   = 1912671
local uly   = 2606606
local xsize = 16384     -- 32 bit pixel, results in 1GB of data
local ysize = 16384


for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:pixels(ulx, uly, xsize, ysize)
    stoptime = time.latch();

    threadCnt = 0
    if tbl ~= nil then
        for j, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
            local cols = v["cols"]
            local rows = v["rows"]
            local size = v["size"]
            local datatype = v["datatype"]
            local ulx = v["ulx"]
            local uly = v["uly"]
            local cellsize = v["cellsize"]
            local wkt = v["wkt"]
            local mbytes = size / (1024*1024)

            if i == 1 then
                print(string.format("AOI size: %6.1f MB,  cols: %6d,  rows: %6d, datatype: %s, ulx: %.2f, uly: %.2f, cellsize: %.9f",
                        mbytes, cols, rows, msg.datatype(datatype), ulx, uly, cellsize))
            end
        end
    end
    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
end


local demType = "arcticdem-mosaic"
print(string.format("\n------------------------------\n%s\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))


-- AOI in pixels
local ulx   = 1071961
local uly   = 2577008
local xsize = 16384     -- 32 bit pixel, results in 1GB of data
local ysize = 16384



for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:pixels(ulx, uly, xsize, ysize)
    stoptime = time.latch();

    threadCnt = 0
    if tbl ~= nil then
        for j, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
            local cols = v["cols"]
            local rows = v["rows"]
            local size = v["size"]
            local datatype = v["datatype"]
            local ulx = v["ulx"]
            local uly = v["uly"]
            local cellsize = v["cellsize"]
            local wkt = v["wkt"]
            local mbytes = size / (1024*1024)

            if i == 1 then
                print(string.format("AOI size: %6.1f MB,  cols: %6d,  rows: %6d, datatype: %s, ulx: %.2f, uly: %.2f, cellsize: %.9f",
                        mbytes, cols, rows, msg.datatype(datatype), ulx, uly, cellsize))
            end
        end
    end

    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
end


-- AOI in pixels
local ulx   = 3360000
local uly   = 366000
local xsize = 32768     -- 8 bit pixel, results in 1GB of data
local ysize = 32768


local demType = "esa-worldcover-10meter"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:pixels(ulx, uly, xsize, ysize)
    stoptime = time.latch();
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    local threadCnt = 0
    for j, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
        local cols = v["cols"]
        local rows = v["rows"]
        local size = v["size"]
        local datatype = v["datatype"]
        local ulx = v["ulx"]
        local uly = v["uly"]
        local cellsize = v["cellsize"]
        local wkt = v["wkt"]
        local mbytes = size / (1024*1024)

        if i == 1 then
            print(string.format("AOI size: %6.1f MB,  cols: %6d,  rows: %6d, datatype: %s, ulx: %.2f, uly: %.2f, cellsize: %.9f",
                    mbytes, cols, rows, msg.datatype(datatype), ulx, uly, cellsize))
        end
    end
    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
end


local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )

