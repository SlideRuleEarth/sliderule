local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()
local json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --

if not core.UNITTEST then
    print("Skipping hls bluetopo self test")
    return
end

-- Setup --
local assets = asset.loaddir()

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms))
while not aws.csget("lpdaac-cloud") do
    print("Waiting to authenticate to LPDAAC...")
    sys.wait(1)
end


local geojsonfile = td.."../data/grand_mesa.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI)\n-------------------------------------------------"))
local demType = "landsat-hls"
local t0str = "2022:01:05:00:00:00"
local t1str = "2022:01:15:00:00:00"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, t0=t0str, t1=t1str, bands = {"NDVI"}, catalog = contents, sort_by_index = true }))
runner.assert(dem ~= nil)

local ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
-- This test ignores lon, lat, lon_incr, lat_incr, pointCount as they are not used.
-- It opens a test file with points.
local pointsFile = td.."../data/grand_mesa_poi.txt"
local pointsInFile = 26183  -- number of points in file
local maxPointCount = 1000  -- number of points to sample, 1000 will trigger all threaded code
status = ut:test(0, 0, 0, 0, maxPointCount, pointsFile);
runner.assert(status, "Failed sampling test")

-- Clean Up --

ut:destroy()

-- Report Results --

runner.report()

