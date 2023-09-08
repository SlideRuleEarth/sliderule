local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

local GDT_datasize = {  1,  --GDT_Byte
                        2,  --GDT_UInt16
                        2,  --GDT_Int16
                        4,  --GDT_UInt32
                        4,  --GDT_Int32
                        4,  --GDT_Float32
                        8,  --GDT_Float64
                        8,  --GDT_CInt16
                        8,  --GDT_CInt32
                        10, --GDT_CFloat32
                        11, --GDT_CFloat64
                        12, --GDT_UInt64
                        13, --GDT_Int64
                        14, --GDT_Int8
                      }

local GDT_dataname = {"GDT_Byte",   "GDT_UInt16", "GDT_Int16",    "GDT_UInt32",   "GDT_Int32",  "GDT_Float32", "GDT_Float64",
                      "GDT_CInt16", "GDT_CInt32", "GDT_CFloat32", "GDT_CFloat64", "GDT_UInt64", "GDT_Int64",   "GDT_Int8"}


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

    for i, v in ipairs(tbl) do
        local cols = v["cols"]
        local rows = v["rows"]
        local datatype = v["datatype"]

        local bytes = cols*rows* GDT_datasize[datatype]
        local mbytes = bytes / (1024*1024)
        -- This results in 12 threads, all the same size, cols, buffs data type. Print only first one
        if i == 1 then
            print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
        end
    end
end


-- Report Results --

runner.report()

