local runner = require("test_executive")
local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Self Test --

runner.unittest("ArcticDEM mosaics sampling ", function()

    local generator = require("arctictdem_test_points_generator")
    local maxPoints = 1000  -- Do not change this value, the generated points are fixed and samples are deterministic
    local lons, lats, heights = generator.generate_points(maxPoints)

    local starttime = time.latch()
    local dem = geo.raster(geo.parms({ asset = "arcticdem-mosaic", algorithm = "NearestNeighbour", radius = 0 }))
    local tbl, err = dem:batchsample(lons, lats, heights)
    runner.assert(err == 0, "Batch sampling failed", true)

    local dtime = time.latch() - starttime
    print(string.format("Batch sampling: %d points read, time: %f", maxPoints, dtime))

    local fileio = require("samples_fileio")
    local luatable = td .. "../data/arcticdem_mosaic_results_1k_points.lua.gz"
    local expectedSamples = fileio.load_results_table(luatable)

    local mismatches = fileio.compare_samples_tables(expectedSamples, tbl)
    print(string.format("Detected %d mismatches", mismatches))
end)

-- Report Results --

runner.report()
