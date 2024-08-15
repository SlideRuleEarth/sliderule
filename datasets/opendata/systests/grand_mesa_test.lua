local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --

local assets = asset.loaddir()
local _,td = runner.srcscript()
local poifile = td.."../../landsat/data/grand_mesa_poi.txt"
local f = io.open(poifile, "r")
-- read in array of POI from file
local arr = {}
for l in f:lines() do
  local row = {}
  for snum in l:gmatch("(%S+)") do
    table.insert(row, tonumber(snum))
  end
  table.insert(arr, row)
end
f:close()

-- show the collected array
--[[
for i=1, 10 do
    print( string.format("%.16f  %.16f", arr[i][1], arr[i][2]))
end
]]

print(string.format("\n-------------------------------------------------\nesa-worldcover 10meter Grand Mesa test\n-------------------------------------------------"))

local demType = "esa-worldcover-10meter"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0 }))

local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"
local failedRead = 0
local samplesCnt = 0
local errorChecking = true

local starttime = time.latch();

-- for i=1,#arr do
for i=1, 5 do
    local  lon = arr[i][1]
    local  lat = arr[i][2]
    local  height = 0
    local  tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        failedRead = failedRead + 1
        print(string.format("======> FAILED to read", lon, lat))
    else
        local value, fname, time
        for j, v in ipairs(tbl) do
            value = v["value"]
            fname = v["file"]
            time  = v["time"]
            print(string.format("(%02d) value %12.3f, time %.2f, fname: %s", j, value, time, fname))
        end
            print("\n")
    end

    samplesCnt = samplesCnt + 1

    local modulovalue = 3000
    if (i % modulovalue == 0) then
        print(string.format("Sample: %d", samplesCnt))
    end

end

local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("\nSamples: %d, failedRead: %d", samplesCnt, failedRead))
print(string.format("ExecTime: %f", dtime))

os.exit()
