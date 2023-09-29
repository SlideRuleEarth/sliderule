local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local sigma = 1.0e-9

-- AOI extent
local llx =  149.80
local lly =  -70.00
local urx =  150.00
local ury =  -69.95

local demTypes = {"rema-mosaic", "rema-strips"}
for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0}))
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s AOI subset\n--------------------------------", demType))

    local starttime = time.latch();
    local tbl, err = dem:subset(llx, lly, urx, ury)
    local stoptime = time.latch();

    runner.check(err == 0)
    runner.check(tbl ~= nil)

    local threadCnt = 0
    for j, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
    print(string.format("subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if demType == "rema-mosaic" then
        runner.check(threadCnt == 1)
    else
        runner.check(threadCnt == 11)  -- 12 threads will be sampled, one will be out of bounds after map projection
    end

    if tbl ~= nil then
        for j, v in ipairs(tbl) do
            local cols = v["cols"]
            local rows = v["rows"]
            local size = v["size"]
            local datatype = v["datatype"]
            local ulx = v["ulx"]
            local uly = v["uly"]
            local cellsize = v["cellsize"]
            local wkt = v["wkt"]

            local mbytes = size / (1024*1024)

            print(string.format("AOI size: %.1f MB, cols: %d, rows: %d, datatype: %s, ulx: %.9f, uly: %.9f, cellsize: %.1f",
                    mbytes, cols, rows, msg.datatype(datatype), ulx, uly, cellsize))

            -- NOTE: mosaic and strips read the same 'area' the difference is the actual data
            --       this test does not have any subset area clipping, only one strips out of 12 was out of bounds
            runner.check(cols == 1915)
            runner.check(rows == 4343)
            runner.check(math.abs(ulx -  1100044.752507064) < sigma)
            runner.check(math.abs(uly - -1896646.081268538) < sigma)

            runner.check(msg.datatype(datatype) == "FLOAT")
            runner.check(cellsize == 2.0)
            runner.check(wkt ~= "")

            if demType == "rema-mosaic" then
                -- TODO: test data
            else
                -- TODO: test data
            end
        end
    end
end


-- Report Results --

runner.report()

