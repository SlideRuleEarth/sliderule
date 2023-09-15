local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --


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
    local tbl, status = dem:subset(llx, lly, urx, ury)
    local stoptime = time.latch();

    runner.check(status == true)
    runner.check(tbl ~= nil)

    local threadCnt = 0
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
    print(string.format("subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if demType == "rema-mosaic" then
        runner.check(threadCnt == 1)
    else
        runner.check(threadCnt == 12)
    end

    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            local cols = v["cols"]
            local rows = v["rows"]
            local size = v["size"]
            local datatype = v["datatype"]

            local mbytes = size / (1024*1024)

            if i == 1 then
                print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, msg.datatype(datatype)))
            end

            runner.check(cols > 0)
            runner.check(rows > 0)
            runner.check(size > 0)
            runner.check(datatype > 0)
        end
    end
end


-- Report Results --

runner.report()

