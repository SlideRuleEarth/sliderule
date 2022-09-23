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
samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}

print('\n------------------\nTest: Strip Elevations\n------------')

local lon = -74.60
local lat = 82.86

for i = 1, 8 do
    local dem = arcticdem.raster("mosaic", samplingAlgs[i])
    local el, status = dem:elevation(lon, lat)
    print(string.format("%20s  %f", samplingAlgs[i], el))
end



os.exit()


-------------


for dems = 1, 2 do

    local lon = -74.60
    local lat =  82.86
    local el, status
    local dem

    print('\n------------------\nTest: Reading milion points\n------------')

    if dems == 1 then
        dem = arcticdem.raster("mosaic", "NearestNeighbour")
    else
        dem = arcticdem.raster("strip", "NearestNeighbour")
    end

    runner.check(dem ~= nil)

    local starttime = time.latch();
    for i = 1, 1000000
    do
        el, status = dem:elevation(lon, lat)
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
    el, status = dem:elevation(lon, lat)
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

