local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local  lon = 180.00
local  lat = 66.34  -- Arctic Circle lat

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}
local demType = "arcticdem-mosaic"

print(string.format("\n-------------------------------------------------\nTest %s: Zonal Stats\n-------------------------------------------------", demType))
for radius = 50, 100, 50
do
    for i = 1, 8 do
        local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlgs[i], radius=radius, zonal_stats=true}))
        local tbl, status = dem:sample(lon, lat)
        if status ~= true then
            print(string.format("======> FAILED to read",lon, lat))
        else
            local el, cnt, min, max, mean, median, stdev, mad, file
            for j, v in ipairs(tbl) do
                el = v["value"]
                cnt = v["count"]
                min = v["min"]
                max = v["max"]
                mean = v["mean"]
                median = v["median"]
                stdev = v["stdev"]
                mad = v["mad"]
                fname = v["file"]
                print(string.format("%20s radius: (%03d) val: %.2f cnt: %d, min: %.2f max: %.2f mean: %.2f median: %.2f stdev: %.2f  mad: %.2f", samplingAlgs[i], radius, el, cnt, min, max, mean, median, stdev, mad))
            end
        end
    end
    print("\n")
end

-- os.exit()

local samplingAlg = "NearestNeighbour"
demType = "arcticdem-strips"
lon = -178.0
lat =   51.8

print(string.format("\n-------------------------------------------------\nTest %s: Zonal Stats\n-------------------------------------------------", demType))
for radius = 50, 100, 50
do
    local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlg, radius=radius, zonal_stats=true}))
    local tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read",lon, lat))
    else
        local el, cnt, min, max, mean, median, stdev, mad, file
        for j, v in ipairs(tbl) do
            el = v["value"]
            cnt = v["count"]
            min = v["min"]
            max = v["max"]
            mean = v["mean"]
            median = v["median"]
            stdev = v["stdev"]
            mad = v["mad"]
            fname = v["file"]
            print(string.format("(%02d) radius: (%03d) val: %.2f cnt: %d, min: %.2f max: %.2f mean: %.2f median: %.2f stdev: %.2f  mad: %.2f", j, radius, el, cnt, min, max, mean, median, stdev, mad))
        end
    end
    print('\n-------------------------------------------------')
end

os.exit()
