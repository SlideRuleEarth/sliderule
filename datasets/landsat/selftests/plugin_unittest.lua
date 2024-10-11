local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()
local json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --
local assets = asset.loaddir()

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms))
while not aws.csget("lpdaac-cloud") do
    print("Waiting to authenticate to LPDAAC...")
    sys.wait(1)
end


local geojsonfile = td.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  lon = -179.0
local  lat = 51.0

local lon_incr = 0.01
local lat_incr = 0.00
local pointCount = 100

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI)\n-------------------------------------------------"))
local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog=contents, sort_by_index=true }))
runner.check(dem ~= nil)

local ut = geo.ut_sample(dem)
runner.check(ut ~= nil)
local status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.check(status, "Failed sampling test")

-- Clean Up --

ut:destroy()

-- Report Results --

runner.report()

