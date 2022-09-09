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

for ctype = 0, 2
do
    local lat =  82.86
    local lon = -74.60
    local el, status

    local arcdem = arcticdem.raster(ctype)
    runner.check(arcdem ~= nil)

    for i = 1, 10
    do
        el, status = arcdem:subset(lon, lat)
        print(i, status, el)
        lat = lat + 0.0001
        lon = lon + 0.0001
    end

    arcdem = nil
end



print('\n------------------\nTest01: ArcticDEM Reader \n------------------')


-- Clean Up --

-- Report Results --

runner.report()

