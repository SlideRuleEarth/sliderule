local runner = require("test_executive")
console = require("console")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local geojsonfile = td.."../selftests/grandmesa.geojson"

-- Unit Test --

local f = io.open(geojsonfile, "r")
local vectorfile = nil

if f ~= nil then
    vectorfile = f:read("*a")
    f:close()
else
    runner.check(false, "failed to open geojson file")
end

local poifile = td.."../../plugins/landsat/data/grand_mesa_poi.txt"
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


local len = string.len(vectorfile)
local cellsize = 0.01
local params = {data = vectorfile, length = len, cellsize = cellsize}

local robj = geo.geojson(params)
runner.check(robj ~= nil)


print('\n------------------\nPerformance test\n------------------')

local maxLoopCnt = 10
local samplesCnt = 0
local nansCnt = 0
local onesCnt = 0
local zerosCnt = 0

local starttime = time.latch();

for loopCnt=1, maxLoopCnt do
    for i=1,#arr do
        local  lon = arr[i][1]
        local  lat = arr[i][2]
        local  height = 0
        local  tbl, status = robj:sample(lon, lat, height)
        if status ~= true then
            print(string.format("======> FAILED to read", lon, lat, height))
        else
            for j, v in ipairs(tbl) do
                s = v["value"]
                fname = v["file"]

                if s == 0 then
                    zerosCnt = zerosCnt + 1
                elseif s == 1 then
                    onesCnt = onesCnt + 1
                else
                    nansCnt = nansCnt + 1
                end
            end
            samplesCnt = samplesCnt + 1
            -- print(string.format("sample %d at lon: %.2f, lat: %.2f is %.2f, %s",i, lon, lat, s, fname))
        end
    end
end

local stoptime = time.latch();
local dtime = stoptime - starttime

runner.check(samplesCnt == maxLoopCnt * #arr)

print(string.format("all samples %d, nans: %d, ones: %d, zeros: %d", samplesCnt, nansCnt, onesCnt, zerosCnt))
print(string.format("duration: %.6f", dtime))
-- sys.quit()

runner.report()


