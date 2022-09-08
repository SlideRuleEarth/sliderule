local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local asset_name = "arcticdem-local"
local arcticdem_local = core.getbyname(asset_name)

-- Unit Test --

print('\n------------------\nTest01: Create ArcticDemRaster \n------------------')
local robj = arcticdem.raster()
runner.check(robj ~= nil)

local lat = 82.86
local lon = -74.60


for i = 10, 1, -1
do
    local el, status = robj:subset(lon, lat)
    print(status, el)
    lat = lat + 0.0001
    lon = lon + 0.0001
end


print('\n------------------\nTest01: ArcticDEM Reader \n------------------')


-- Clean Up --

-- Report Results --

runner.report()

