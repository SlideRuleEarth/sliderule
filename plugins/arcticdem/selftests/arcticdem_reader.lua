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

print('\n------------------\nMilion Points All In Cache\n------------------')

for ctype = 0, 2
do
    local lat =  82.86
    local lon = -74.60
    local el, status

    local arcdem = arcticdem.raster(ctype)
    runner.check(arcdem ~= nil)

    local starttime = time.latch();
    for i = 1, 1000000
    do
        el, status = arcdem:subset(lon, lat)
        if status ~= true then
            print(i, status, el)
        end
        if (i % 2 == 0) then
            lon = lon + 0.0001
         -- lat = lat + 0.0001
        else
            lon = lon - 0.0001
         -- lat = lat - 0.0001
        end
    end
    local stoptime = time.latch();
    local dtime = stoptime-starttime
    print('ExecTIme:',dtime*1000, '\n')

    arcdem = nil
end


print('\n------------------\n10 Points All Cache Misses\n------------------')

for ctype = 0, 2
do
    local lat =  82.86
    local lon = -74.60
    local el, status

    local arcdem = arcticdem.raster(ctype)
    runner.check(arcdem ~= nil)

    local starttime = time.latch();
    -- for i = 1, 1000000
    for i = 1, 10
    do
        el, status = arcdem:subset(lon, lat)
        if status ~= true then
            print(i, status, el)
        end
        if (i % 2 == 0) then
            lon = lon + 0.2
            lat = lat + 0.2
        else
            lon = lon - 0.2
            lat = lat - 0.2
        end
    end
    local stoptime = time.latch();
    local dtime = stoptime-starttime
    print('ExecTIme:',dtime*1000, '\n')

    arcdem = nil
end


os.exit()


print('\n------------------\nTest01: ArcticDEM Reader \n------------------')


-- Clean Up --

-- Report Results --

runner.report()

