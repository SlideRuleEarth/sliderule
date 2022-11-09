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

local lon = -74.60
local lat = 82.86

print('\n------------------\nTest: AWS mosaic\n------------')
local dem = arcticdem.raster("mosaic", "NearestNeighbour", 0)
local starttime = time.latch();
local tbl, status = dem:samples(lon, lat)
local stoptime = time.latch();
local dtime = stoptime - starttime

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    print(i, el, fname)
end
print('ExecTime:', dtime * 1000, '\n')
print('********** Exepcted result is:\n')
print('1	440.092803955081	34_37_1_1_2m_v3.0_reg_dem.tif')

-- os.exit()

local el, status

print('\n------------------\nTest: Reading milion points\n------------')

starttime = time.latch();
for i = 1, 1000000
-- for i = 1, 1000
do
    el, status = dem:sample(lon, lat)
    if status ~= true then
        print(i, status, el)
    end
    if (i % 2600 == 0) then
        lon = -74.60
        lat =  82.86
    else
        lon = lon + 0.0001
        lat = lat + 0.0001
    end

    -- if (i % 50 == 0) then
    --     print('Running:', i)
    -- end

end

stoptime = time.latch();
dtime = stoptime-starttime
print('ExecTime:', dtime*1000)


os.exit()

-- Clean Up --
-- Report Results --

runner.report()

