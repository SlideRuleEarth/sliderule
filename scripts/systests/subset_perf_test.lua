local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")
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


-- AOI extent (extent of grandmesa.geojson)
local gm_llx = -108.3412
local gm_lly =   38.8236
local gm_urx = -107.7292
local gm_ury =   39.1956


--[[
--]]

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
threadCnt = 0
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

starttime = time.latch();
tbl, status = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
stoptime = time.latch();
runner.check(status == true)

local threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

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
threadCnt = 0
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))


-- AOI extent
local llx =  149.81
local lly =   69.81
local urx =  150.00
local ury =   70.00

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();
runner.check(status == true)

threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

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
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))


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
threadCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    -- print(string.format("(%02d) %10.2f  %s", i, el, fname))
    threadCnt = threadCnt + 1
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

-- AOI extent
llx =  149.80
lly =  -70.00
urx =  150.01
ury =  -69.93

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();
runner.check(status == true)

threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

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


demType = "landsat-hls"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)


local geojsonfile = td.."../../plugins/landsat/data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

local  lon = -179.0
local  lat = 51.0
local  height = 0

dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog = contents }))
local sampleCnt = 0
local ndvi = 0
local gpsTime = 0


starttime = time.latch();
tbl, status = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)

threadCnt = 0
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for i, v in ipairs(tbl) do
        value = v["value"]
        time  = v["time"]
        fname = v["file"]
        -- print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, time, fname))
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)\n", stoptime - starttime, threadCnt))

starttime = time.latch();
tbl, status = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
stoptime = time.latch();
runner.check(status == true)

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

if tbl ~= nil then
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
end


demType = "usgs3dep-1meter-dem"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local geojsonfile = td.."../../plugins/usgs3dep/data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

local  lon =    -108.1
local  lat =      39.1
local  height = 2630.0

local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))

starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)

threadCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    -- print(string.format("(%02d) %10.2f  %s", i, el, fname))
    threadCnt = threadCnt + 1
end
print(string.format("POI sample time: %.2f   (%d threads)\n", stoptime - starttime, threadCnt))


starttime = time.latch();
tbl, status = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
stoptime = time.latch();
runner.check(status == true)

threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local data = v["data"]
        local cols = v["cols"]
        local rows = v["rows"]
        local datatype = v["datatype"]

        local bytes = cols*rows* GDT_datasize[datatype]
        local mbytes = bytes / (1024*1024)
        -- print(string.format("(%02d) dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", i, data, bytes, mbytes, cols, rows, datatype))
        print(string.format("AOI subset datasize: %8.2f MB, cols: %6d, rows: %6d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
    end
end


os.exit()

runner.report()


