local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/arcticdem_strips.json"
local contents = readgeojson.load(jsonfile)

-- Self Test --

local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

local sigma = 1.0e-9

-- AOI extent
local llx = -149.90
local lly = 70.10
local urx = -149.80
local ury = 70.15
local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}

for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=50, zonal_stats=true}))
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

    if demType == "arcticdem-mosaic" then
        runner.assert(threadCnt == 1)
    else
        runner.assert(threadCnt == 25)  -- 25 threads will be sampled
    end
    if tbl ~= nil then
        for j, v in ipairs(tbl) do
            local size     = v["size"]
            local poolsize = v["poolsize"]
            local size_mbytes = size / (1024*1024)
            local poolsize_mbytes = poolsize / (1024*1024)

            print(string.format("AOI size: %d bytes (%.1f MB), poolsize: %.1f MB", size, size_mbytes, poolsize_mbytes))

            -- NOTE: mosaic and strips read the same 'area' the difference is the actual data
            --       all but one thread read the same size data (22503936 bytes) one thread read 19280256 bytes
            runner.assert(size == 22503936 or size == 19280256)
        end
    end
end

-- Report Results --

runner.report()
