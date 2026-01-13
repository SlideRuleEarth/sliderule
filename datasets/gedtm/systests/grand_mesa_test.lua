local runner = require("test_executive")

-- Setup --

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

print(string.format("\n-------------------------------------------------\nGEDTM 30 meter Grand Mesa test\n-------------------------------------------------"))

local demType = "gedtm-30meter"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0 }))

local failedRead = 0
local samplesCnt = 0
local errorChecking = true

local starttime = time.latch();

local lons = {}
local lats = {}
local heights = {}
for i=1,#arr do
    lons[i] = arr[i][1]
    lats[i] = arr[i][2]
    heights[i] = 0
end

local  tbl, err = dem:batchsample(lons, lats, heights)
if err ~= 0 then
    print("======> FAILED to batchsample")
else
    for i=1,#arr do
        local  lon = lons[i]
        local  lat = lats[i]
        local  pointSamples = tbl[i]
        if pointSamples == nil then
            failedRead = failedRead + 1
            print(string.format("======> FAILED to read %0.16f, %0.16f", lon, lat))
        else
            local value, fname, time
            for j, v in ipairs(pointSamples) do
                value = v["value"]
                fname = v["file"]
                time  = v["time"]
                -- print(string.format("value %12.3f, time %.2f, fname: %s", value, time, fname))
            end
        end

        samplesCnt = samplesCnt + 1

        local modulovalue = 5000
        if (i % modulovalue == 0) then
            print(string.format("Sample: %d", samplesCnt))
        end

    end
end

local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("\nSamples: %d, failedRead: %d", samplesCnt, failedRead))
print(string.format("ExecTime: %f", dtime))

os.exit()
