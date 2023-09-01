local runner = require("test_executive")
console = require("console")
asset = require("asset")
local td = runner.rootdir(arg[0])

-- Setup --
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

local GDT_dataname = {  "GDT_Byte",
                        "GDT_UInt16",
                        "GDT_Int16",
                        "GDT_UInt32",
                        "GDT_Int32",
                        "GDT_Float32",
                        "GDT_Float64",
                        "GDT_CInt16",
                        "GDT_CInt32",
                        "GDT_CFloat32",
                        "GDT_CFloat64",
                        "GDT_UInt64",
                        "GDT_Int64",
                        "GDT_Int8",
                      }


local demType = "esa-worldcover-10meter"

print(string.format("\n--------------------------\n%s\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
local lon =    -108.1
local lat =      39.1
local height =    0.0
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)
print(string.format("POI sample time: %.2f", stoptime - starttime))

-- AOI extent
local llx = -108.3412
local lly =   38.8236
local urx = -107.7292
local ury =   39.1956

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();
print(string.format("AOI subset time: %.2f", stoptime - starttime))
runner.check(status == true)

for i, v in ipairs(tbl) do
    local data = v["data"]
    local time = v["time"]
    local cols = v["cols"]
    local rows = v["rows"]
    local datatype = v["datatype"]

    local bytes = cols*rows* GDT_datasize[datatype]
    local mbytes = bytes / (1024*1024)
    -- print(string.format("(%02d) dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", i, data, bytes, mbytes, cols, rows, datatype))
    print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
end


demType = "arcticdem-mosaic"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
lon = -150.0
lat =   70.0
height = 0
starttime = time.latch();
tbl, status = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)
print(string.format("POI sample time: %.2f", stoptime - starttime))

-- AOI extent
llx =  149.81
lly =   69.81
urx =  150.00
ury =   70.00

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();
print(string.format("AOI subset time: %.2f", stoptime - starttime))
runner.check(status == true)

for i, v in ipairs(tbl) do
    local data = v["data"]
    local cols = v["cols"]
    local rows = v["rows"]
    local datatype = v["datatype"]

    local bytes = cols*rows* GDT_datasize[datatype]
    local mbytes = bytes / (1024*1024)
    -- print(string.format("(%02d) dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", i, data, bytes, mbytes, cols, rows, datatype))
    print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
end

demType = "rema-mosaic"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
lon = -170
lat =  -85
starttime = time.latch();
tbl, status = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)
print(string.format("POI sample time: %.2f", stoptime - starttime))

-- AOI extent
llx =  149.80
lly =  -70.00
urx =  150.01
ury =  -69.93

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();
print(string.format("AOI subset time: %.2f", stoptime - starttime))
runner.check(status == true)

for i, v in ipairs(tbl) do
    local data = v["data"]
    local cols = v["cols"]
    local rows = v["rows"]
    local datatype = v["datatype"]

    local bytes = cols*rows* GDT_datasize[datatype]
    local mbytes = bytes / (1024*1024)
    -- print(string.format("(%02d) dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", i, data, bytes, mbytes, cols, rows, datatype))
    print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
end

os.exit()

runner.report()


