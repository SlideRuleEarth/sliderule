local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local lon = -178.0
local lat =   51.7

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}

print('\n------------------\nTest: Sampling Elevations\n------------------')

for radius = 0, 8 do
    for i = 1, 8 do
        dem = geo.vrt("arcticdem-mosaic", samplingAlgs[i], radius)
        tbl, status = dem:sample(lon, lat)

        local el, file
        for i, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
        end

        print(string.format("%20s (%02d) %15f", samplingAlgs[i], radius, el))
    end
    print('\n-------------------------------------------------')
end

os.exit()
