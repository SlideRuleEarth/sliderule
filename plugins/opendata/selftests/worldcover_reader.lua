local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()
-- Unit Test --

local  sigma = 1.0e-9
local  lon =    -108.1
local  lat =      39.1
local  height =    0.0


print(string.format("\n-------------------------------------------------\nesa worldcover 10meter sample POI\n-------------------------------------------------"))

local expResults = {{10.0, 1309046418000, '/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt'}}

local demType = "esa-worldcover-10meter"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.check(status == true)
runner.check(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.check(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.check(time == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)

print(string.format("\n-------------------------------------------------\nesa worldcover 10meter subset AOI\n-------------------------------------------------"))
starttime = time.latch();
local data, cols, rows, datatype, status = dem:subset(-108.3412, 39.1956, -107.7292, 38.8236)
stoptime = time.latch();
print(string.format("subset time: %f", stoptime - starttime))
local bytes = cols*rows*1
local mbytes = bytes / (1024*1024)
print(string.format("subset dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", data, bytes, mbytes, cols, rows, datatype))

runner.check(status == true)
runner.check(data ~= nil)
runner.check(cols == 7344)
runner.check(rows == 4464)
runner.check(datatype == 1)  -- GDT_Byte


-- Report Results --

runner.report()

