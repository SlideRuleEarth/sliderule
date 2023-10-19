local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local loopcnt = 5


local demType = "rema-mosaic"
print(string.format("\n------------------------------\n%s\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

local llx =  148.50
local lly =  -70.00
local urx =  150.00
local ury =  -69.50


for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:subset(llx, lly, urx, ury)
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


-- AOI extent
local llx =  -105.00
local lly =   70.40
local urx =  -104.50
local ury =   71.00


for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:subset(llx, lly, urx, ury)
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



local llx = 100.0
local lly =  50.0
local urx = 103.5
local ury =  53.5

local demType = "esa-worldcover-10meter"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:subset(llx, lly, urx, ury)
    stoptime = time.latch();
    runner.check(err == 0)
    runner.check(tbl ~= nil)

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




-- AOI extent (extent of grandmesa.geojson)
local gm_llx = -108.3412
local gm_lly =   38.8236
local gm_urx = -107.7292
local gm_ury =   39.1956

local demType = "esa-worldcover-10meter"
print(string.format("\n--------------------------\n%s Grandmesa\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

for i=1, loopcnt do
    starttime = time.latch();
    tbl, err = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
    stoptime = time.latch();

    runner.check(err == 0)
    runner.check(tbl ~= nil)

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

