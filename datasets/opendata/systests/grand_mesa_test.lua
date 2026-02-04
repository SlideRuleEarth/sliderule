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

local lons = {}
local lats = {}
local heights = {}
for i=1,#arr do
    lons[i] = arr[i][1]
    lats[i] = arr[i][2]
    heights[i] = 0
end

local function run_test(demType, testName)
    print(string.format("Running test: %s (%s)", testName, demType))

    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0 }))
    local starttime = time.latch()
    local tbl, err = dem:batchsample(lons, lats, heights)
    local stoptime = time.latch()

    local sampled = 0
    if err == 0 and tbl ~= nil then
        for i=1,#arr do
            if tbl[i] ~= nil then
                sampled = sampled + 1
            end
        end
    end

    print(string.format("Points sampled: %d/%d", sampled, #arr))
    print(string.format("ExecTime: %f", stoptime - starttime))
end

run_test("esa-worldcover-10meter", "Grand Mesa WorldCover")
run_test("esa-copernicus-30meter", "Grand Mesa Copernicus")

-- Report Results --

runner.report()
