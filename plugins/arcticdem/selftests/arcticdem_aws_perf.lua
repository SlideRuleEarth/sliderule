local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local asset_name = "arcticdem-local"
local arcticdem_local = core.getbyname(asset_name)


local  lon = -178.0
local  lat =   51.7
local _lon = lon
local _lat = lat

--[[
--]]
print('\n------------------\nTest: AWS mosaic\n------------')
local dem = arcticdem.raster("mosaic", "NearestNeighbour", 0)
local starttime = time.latch();
local tbl, status = dem:samples(lon, lat)
local stoptime = time.latch();
local dtime = stoptime - starttime

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    print(string.format("(%02d) %20f     %s", i, el, fname))
end
print('ExecTime:', dtime * 1000, '\n')

local el, status
local max_cnt = 1000000

print('\n------------------\nTest: Reading', max_cnt, '  points\n------------')

starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        print(i, status)
    end

    -- Keep all points in the same mosaic raster (may be different for strips)
    if (i % 2000 == 0) then
        lon = _lon
        lat = _lat
    else
        lon = lon + 0.0001
        lat = lat + 0.0001
    end

    modulovalue = 100000
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print('Points read:', i, dtime*1000)
        intervaltime = time.latch();
    end

end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(max_cnt, 'points read time', dtime*1000, 'msecs')

-- os.exit()


local lon = -178.0
local lat =   51.7

print('\n------------------\nTest: AWS strip\n------------')
local dem = arcticdem.raster("strip", "NearestNeighbour", 0)
local starttime = time.latch();
local tbl, status = dem:samples(lon, lat)
local stoptime = time.latch();
local dtime = stoptime - starttime

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    print(string.format("(%02d) %20f     %s", i, el, fname))
end
print('ExecTime:', dtime * 1000, '\n')

local el, status
local max_cnt = 1000000
-- local max_cnt = 5000

print('\n------------------\nTest: Reading', max_cnt, '  points\n------------')

starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        print(i, status)
    end

    -- if (i % 2000 == 0) then
    --     lon = _lon
    --     lat = _lat
    -- else
    --     lon = lon + 0.0001
    --     lat = lat + 0.0001
    -- end

    modulovalue = 100000
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print('Points read:', i, dtime*1000)
        intervaltime = time.latch();
    end

end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(max_cnt, 'points read time', dtime*1000, 'msecs')


os.exit()

-- Clean Up --
-- Report Results --

runner.report()

