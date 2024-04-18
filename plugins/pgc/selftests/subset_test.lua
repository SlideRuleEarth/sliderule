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

local lon = 149.90
local lat = -69.97
local height = 0

-- local demTypes = {"rema-mosaic", "rema-strips"}
local demTypes = {"rema-mosaic"}
for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="Cubic"}))  -- Resample the subset data with cubic algorithm
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s AOI subset\n--------------------------------", demType))

    local tbl, err = dem:sample(lon, lat, height)
    runner.check(err == 0)
    runner.check(tbl ~= nil)

    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        print(string.format("(%02d) %8.2f %s", i, el, fname))
        sampleCnt = sampleCnt + 1
    end

    -- local starttime = time.latch();
    -- local dem= dem:subset(llx, lly, urx, ury)
    -- local stoptime = time.latch();

    -- if err ~= 0 then
    --     print(string.format("subset error: %d", err))
    -- else
    --     print(string.format("subset time: %.2f", stoptime - starttime))
    -- end

    -- if(dem ~= nil) then
    --     local tbl, err = dem:sample(lon, lat, height)
    --     runner.check(err == 0)
    --     runner.check(tbl ~= nil)

    --     local sampleCnt = 0
    --     for i, v in ipairs(tbl) do
    --         local el = v["value"]
    --         local fname = v["file"]
    --         print(string.format("(%02d) %8.2f %s", i, el, fname))
    --         sampleCnt = sampleCnt + 1
    --     end



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

            local mbytes = size / (1024*1024)

            print(string.format("AOI size: %.1f MB, cols: %d, rows: %d, datatype: %s",
                    mbytes, cols, rows, msg.datatype(datatype)))

            -- NOTE: mosaic and strips read the same 'area' the difference is the actual data
            --       this test does not have any subset area clipping, only one strip out of 12 was out of bounds
            runner.check(cols == 1915)
            runner.check(rows == 4343)
            runner.check(msg.datatype(datatype) == "FLOAT")

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

