local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test For Temporal filter --

local sigma = 1.0e-9

local demType = "arcticdem-mosaic"
local samplingRadius = 30

local  lon = 100.00
local  lat = 66.34  -- Arctic Circle lat
local  height = 0

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}
local minElevation = 638

for radius = 0, 100, 50
do
    print(string.format("\n-------------------------------------------------\nTest %s: Resampling with radius %d meters\n-------------------------------------------------", demType, radius))
    for i = 1, #samplingAlgs do
        local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlgs[i], radius=radius}))
        local tbl, err = dem:sample(lon, lat, height)
        if err ~= 0 then
            print(string.format("======> FAILED to read",lon, lat))
        else
            local el, file
            for j, v in ipairs(tbl) do
                el = v["value"]
                if i == 1 then
                    --NearestNeighbour has always the same value, regardless of sampling radius
                    runner.assert(math.abs(el - 653.148437500) < sigma)
                else
                    runner.assert(el > minElevation)
                end
            end
            print(string.format("%16s %16.9f", samplingAlgs[i], el))
        end
    end
    print('\n-------------------------------------------------')
end

-- Report Results --

runner.report()
