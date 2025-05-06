local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Self Test --

local sigma = 1.0e-9

-- AOI extent
local llx =  149.80
local lly =  -70.00
local urx =  150.00
local ury =  -69.95

local demTypes = {"rema-mosaic", "rema-strips"}
for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
    runner.assert(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s AOI subset\n--------------------------------", demType))

    local starttime = time.latch();
    local tbl, err = dem:subset(llx, lly, urx, ury)
    local stoptime = time.latch();

    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    local threadCnt = 0
    for j, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
    print(string.format("subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if demType == "rema-mosaic" then
        runner.assert(threadCnt == 1)
    else
        runner.assert(threadCnt == 11)  -- 12 threads will be sampled, one will be out of bounds after map projection
    end

    if tbl ~= nil then
        for j, v in ipairs(tbl) do
            local size     = v["size"]
            local poolsize = v["poolsize"]
            local size_mbytes = size / (1024*1024)
            local poolsize_mbytes = poolsize / (1024*1024)

            print(string.format("AOI size: %d bytes (%.1f MB), poolsize: %.1f MB", size, size_mbytes, poolsize_mbytes))

            -- NOTE: mosaic and strips read the same 'area' the difference is the actual data
            --       this test does not have any subset area clipping, only one strip out of 12 was out of bounds
            runner.assert(size ==  33267380)

            -- TODO: get the raster object and sample it
        end
    end
end

-- Report Results --

runner.report()
