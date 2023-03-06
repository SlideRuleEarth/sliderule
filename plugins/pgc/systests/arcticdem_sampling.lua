local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in

-- Unit Test --

local  lon = 180.00
local  lat = 66.34  -- Arctic Circle lat

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}

demType = "arcticdem-mosaic"
for radius = 0, 100, 50
do
    print(string.format("\n-------------------------------------------------\nTest %s: Resampling with radius %d\n-------------------------------------------------", demType, radius))
    for i = 1, 8 do
        local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlgs[i], radius=radius}))
        local tbl, status = dem:sample(lon, lat)
        if status ~= true then
            print(string.format("======> FAILED to read",lon, lat))
        else
            local el, file
            for j, v in ipairs(tbl) do
                el = v["value"]
                fname = v["file"]
            end
            print(string.format("%20s (%02d) %15f", samplingAlgs[i], radius, el))
        end
    end
    print('\n-------------------------------------------------')
end

demType = "arcticdem-strips"
lon = -178.0
lat =   51.8

for radius = 0, 100, 50
do
    print(string.format("\n-------------------------------------------------\nTest %s: Resampling with radius %d\n-------------------------------------------------", demType, radius))
    for i = 1, 8 do
        local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlgs[i], radius=radius}))
        local tbl, status = dem:sample(lon, lat)
        if status ~= true then
            print(string.format("======> FAILED to read",lon, lat))
        else
            local el, file
            for j, v in ipairs(tbl) do
                el = v["value"]
                fname = v["file"]
                print(string.format("%02d %17s (%02d) %15f", j, samplingAlgs[i], radius, el))
                break
            end
        end
    end
    print('\n-------------------------------------------------')
end

sys.quit()
