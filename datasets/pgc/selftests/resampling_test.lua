local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

-- Self Test --

local sigma = 1.0e-9

local demType = "arcticdem-mosaic"
local samplingRadius = 30

local lon = -150.0
local lat =   70.0
local height = 0

local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}
local minElevation = 100

for radius = 0, 100, 50
do
    print(string.format("\n-------------------------------------------------\nTest %s: Resampling with radius %d meters\n-------------------------------------------------", demType, radius))
    for i = 1, #samplingAlgs do
        local dem = geo.raster(geo.parms({asset=demType, algorithm=samplingAlgs[i], radius=radius}))
        local tbl, err = dem:sample(lon, lat, height)
        runner.assert(err == 0, string.format("failed to read: %f %f", lon, lat))
        runner.assert(#tbl > 0, string.format("failed to return any samples: %f %f", lon, lat))
        if (err == 0 and #tbl > 0) then
            local el, file
            for j, v in ipairs(tbl) do
                el = v["value"]
                if i == 1 then
                    --NearestNeighbour has always the same value, regardless of sampling radius
                    runner.assert(math.abs(el - 116.250) < sigma)
                else
                    runner.assert(el > minElevation)
                end
            end
        end
    end
    print('\n-------------------------------------------------')
end

-- Report Results --

runner.report()
