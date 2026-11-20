local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local base64 = require("base64")

-- Setup --

local sigma = 1.0e-6

--[[
    ../data/aspect_azimuth_planes.tif is a synthetic 204x51 raster of 1 meter
    pixels in EPSG:32612, made of four 51x51 blocks, each a perfect plane with a
    0.125 m/m gradient per axis (every elevation is exact in float32):

        block 0  downslope due east       -> compass azimuth  90
        block 1  downslope due north      -> compass azimuth   0
        block 2  downslope due southwest  -> compass azimuth 225
        block 3  flat                     -> aspect undefined

    The lon/lat below are the center pixel of each block; gdaldem aspect on the
    same raster returns 90, 0, 225 and its nodata value for the flat block.
]]
local expResults = {{-108.6870184024, 39.0959797083,  90.0, "east"},
                    {-108.6864291160, 39.0959680103,   0.0, "north"},
                    {-108.6858398299, 39.0959563094, 225.0, "southwest"},
                    {-108.6852505442, 39.0959446055, "nan", "flat"}}

-- Self Test --

runner.unittest("Slope Aspect Compass Azimuth", function()
    -- open synthetic raster
    local f = io.open(dirpath.."../data/aspect_azimuth_planes.tif", "rb")
    assert(f)
    local rasterfile = f:read("*a")
    f:close()

    -- open raster
    local encodedRaster = base64.encode( rasterfile )
    local len = string.len(encodedRaster)
    local params = {
        data = encodedRaster,
        length = len,
        date = 0,
        samples = {
            elevation_bands = {"1"},
            slope_aspect = true,
            slope_scale_length = 40
        }
    }
    local robj = geo.userraster(params)
    runner.assert(robj ~= nil, "failed to create user raster", true)

    -- sample the center of each plane
    for i, v in ipairs(expResults) do
        local tbl, err = robj:sample(v[1], v[2], 0)
        runner.assert(err == 0, string.format("failed to sample %s facing plane", v[4]), true)
        runner.assert(tbl ~= nil, string.format("failed to sample %s facing plane", v[4]), true)

        local slope = tbl[1]["slope"]
        local aspect = tbl[1]["aspect"]
        local slope_count = tbl[1]["slope_count"]

        print(string.format("(%02d) %-9s slope: %f  aspect: %f  slope_count: %d",
                            i, v[4], slope, aspect, slope_count))

        runner.assert(slope_count == 1681, string.format("slope_count = %d, expected = 1681", slope_count))

        if v[3] == "nan" then
            -- flat plane, aspect is undefined
            runner.assert(slope == 0.0, string.format("slope = %f, expected = 0.0", slope))
            runner.assert(aspect ~= aspect, string.format("aspect = %f, expected = nan", aspect))
        else
            runner.assert(slope > 0.0, string.format("slope = %f, expected > 0.0", slope))
            runner.assert(math.abs(aspect - v[3]) < sigma, string.format("aspect = %f, expected = %f, error = %f", aspect, v[3], math.abs(aspect - v[3])))
        end
    end
end)

-- Report Results --

runner.report()
