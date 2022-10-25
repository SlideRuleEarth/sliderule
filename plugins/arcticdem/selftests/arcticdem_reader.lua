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

-- Unit Test --

-------------
local dem, el, status

local lon = -74.60
local lat = 82.86

print('\n------------------\nTest: Strip multiple points in time\n------------')
dem = arcticdem.raster("strip", "NearestNeighbour", 0)
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


print('\n------------------\nTest: Mosaic multiple points in time\n------------')
dem = arcticdem.raster("mosaic", "NearestNeighbour", 0)
starttime = time.latch();
tbl, status = dem:samples(lon, lat)
stoptime = time.latch();
dtime = stoptime - starttime

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    print(i, el, fname)
end
print('ExecTime:', dtime * 1000, '\n')

--os.exit()

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}

print('\n------------------\nTest: Sampling Elevations\n------------')

for radius = 0, 5 do
    for i = 1, 8 do
        dem = arcticdem.raster("mosaic", samplingAlgs[i], radius)
        el, status = dem:sample(lon, lat)
        print(string.format("%20s (%02d)  %f", samplingAlgs[i], radius, el))
    end
    print('\n-------------------------------------------')
end

-- os.exit()



for dems = 1, 2 do

    local lon = -74.60
    local lat =  82.86
    local el, status
    local dem

    print('\n------------------\nTest: Reading milion points\n------------')

    if dems == 1 then
        dem = arcticdem.raster("mosaic", "NearestNeighbour", 0)
    else
        dem = arcticdem.raster("strip", "NearestNeighbour", 0)
    end

    runner.check(dem ~= nil)

    local starttime = time.latch();
    for i = 1, 1000000
    do
        el, status = dem:sample(lon, lat)
        if status ~= true then
            print(i, status, el)
        end
        if (i % 1000 == 0) then
            lon = -74.60
            lat =  82.86
        else
            lon = lon + 0.0001
            lat = lat + 0.0001
        end
    end
    local stoptime = time.latch();
    local dtime = stoptime-starttime
    print('ExecTime:',dtime*1000, '\n')


-- 'hole' in the raster
    lat =  82.898092
    lon = -74.418638
    el, status = dem:sample(lon, lat)
    print('hole in raster', status, el)

    print('\n------------------\nTest: dim\n------------------')
    local rows, cols = dem:dim()
    print("rows: ", rows, "cols: ", cols)
    runner.check(rows ~= 0)
    runner.check(cols ~= 0)

    print('\n------------------\nTest: bbox\n------------------')
    local lon_min, lat_min, lon_max, lat_max = dem:bbox()
    print("lon_min: ", lon_min, "lat_min: ", lat_min, "\nlon_max: ", lon_max, "lat_max: ", lat_max)
    runner.check(lon_min ~= 0)
    runner.check(lat_min ~= 0)
    runner.check(lon_max ~= 0)
    runner.check(lon_max ~= 0)

    print('\n------------------\nTest: cellsize\n------------------')
    local cellsize = dem:cell()
    print("cellsize: ", cellsize)
    runner.check(cellsize == 2.0)
end



-- Clean Up --
-- Report Results --

runner.report()

