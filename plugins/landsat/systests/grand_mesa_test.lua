local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --

local assets = asset.loaddir()
local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)


local td = runner.rootdir(arg[0])
local geojsonfile = td.."../data/grand_mesa.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

local poifile = td.."../data/grand_mesa_poi.txt"
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

local ndvifile = td.."../data/grand_mesa_ndvi.txt"
local f = io.open(ndvifile, "r")
-- read in array of NDVI values
local ndvi_results = {}

for l in f:lines() do
  table.insert(ndvi_results, tonumber(l))
end
f:close()

print(string.format("\n-------------------------------------------------\nLandsat Grand Mesa test\n-------------------------------------------------"))

local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, closest_time = "2022-01-05T00:00:00Z", bands = {"NDVI"}, catalog = contents }))

local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"
local badFileCnt = 0
local badNDVICnt = 0
local samplesCnt = 0
local errorChecking = true

local starttime = time.latch();

for i=1,#arr do
-- for i=1, 14 do
    local  lon = arr[i][1]
    local  lat = arr[i][2]
    local  tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read", lon, lat))
    else
        local ndvi, fname
        for j, v in ipairs(tbl) do
            ndvi = v["value"]
            fname = v["file"]

            if errorChecking then
                if fname ~= expectedFile then
                    print(string.format("======> wrong group: %s", fname))
                    badFileCnt = badFileCnt + 1
                end

                local expectedNDVI = ndvi_results[i]
                local delta = 0.0000000000000001
                local min = expectedNDVI - delta
                local max = expectedNDVI + delta
                if (ndvi > max or ndvi < min) then
                    print(string.format("======> wrong ndvi: %0.16f, expected: %0.16f", ndvi, expectedNDVI))
                    badNDVICnt = badNDVICnt + 1
                end
            end
            -- print(string.format("%0.16f, fname: %s", ndvi, fname))
        end
    end

    samplesCnt = samplesCnt + 1

    local modulovalue = 3000
    if (i % modulovalue == 0) then
        print(string.format("Sample: %d", samplesCnt))
    end

end

local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("\nSamples: %d, wrong NDVI: %d, wrong groupID: %d", samplesCnt, badNDVICnt, badFileCnt))
print(string.format("ExecTime: %f", dtime))

os.exit()
