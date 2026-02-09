local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local sigma = 1.0e-9

local lon = -150.0
local lat =   70.0
local height = 0

-- Self Test --

runner.unittest("ArcticDEM Resampling Test", function()
    local samplingAlgs = {"NearestNeighbour", "Bilinear", "Cubic", "CubicSpline", "Lanczos", "Average", "Mode", "Gauss"}
    local minElevation = 100
    for radius = 0, 100, 50 do
        for i = 1, #samplingAlgs do
            print(string.format("Sampling with algorithm=%s and radius=%d", samplingAlgs[i], radius))
            local dem = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm=samplingAlgs[i], radius=radius}))
            local tbl, err = dem:sample(lon, lat, height)
            runner.assert(err == 0, string.format("failed to read: %f %f", lon, lat), true)
            runner.assert(#tbl > 0, string.format("failed to return any samples: %f %f", lon, lat), true)
            for _, v in ipairs(tbl) do
                local el = v["value"]
                if i == 1 then
                    --NearestNeighbour has always the same value, regardless of sampling radius
                    runner.assert(math.abs(el - 116.250) < sigma)
                else
                    runner.assert(el > minElevation)
                end
            end
        end
    end
end)

-- Report Results --

runner.report()
